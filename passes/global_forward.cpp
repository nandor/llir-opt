// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>
#include <limits>
#include <queue>
#include <unordered_set>

#include <llvm/ADT/Statistic.h>
#include <llvm/Support/Debug.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/adt/bitset.h"
#include "core/adt/id.h"
#include "core/analysis/call_graph.h"
#include "core/analysis/object_graph.h"
#include "core/analysis/reference_graph.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/dag.h"
#include "core/expr.h"
#include "core/func.h"
#include "core/inst_visitor.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "passes/global_forward.h"

#define DEBUG_TYPE "global-forward"

STATISTIC(NumLoadsFolded, "Loads folded");
STATISTIC(NumStoresFolded, "Stores folded");



// -----------------------------------------------------------------------------
static std::optional<std::pair<Object *, std::optional<uint64_t>>>
GetObject(Ref<Inst> inst)
{
  auto mov = ::cast_or_null<MovInst>(inst);
  if (!mov) {
    return std::nullopt;
  }
  auto arg = mov->GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::CONST:
    case Value::Kind::INST: {
      return std::nullopt;
    }
    case Value::Kind::EXPR: {
      switch (::cast<Expr>(arg)->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto expr = ::cast<SymbolOffsetExpr>(arg);
          if (auto atom = ::cast_or_null<Atom>(expr->GetSymbol())) {
            auto *obj = atom->getParent();
            if (&*obj->begin() == &*atom) {
              return std::make_pair(obj, expr->GetOffset());
            }
            return std::make_pair(obj, std::nullopt);
          }
          return std::nullopt;
        }
      }
      llvm_unreachable("invalid expression kind");
    }
    case Value::Kind::GLOBAL: {
      if (auto atom = ::cast_or_null<Atom>(arg)) {
        auto *obj = atom->getParent();
        if (&*obj->begin() == &*atom) {
          return std::make_pair(obj, 0ull);
        }
        return std::make_pair(obj, std::nullopt);
      }
      return std::nullopt;
    }
  }
  llvm_unreachable("invalid argument kind");
}

// -----------------------------------------------------------------------------
static bool IsConstant(Ref<Value> value)
{
  switch (value->GetKind()) {
    case Value::Kind::INST: {
      return false;
    }
    case Value::Kind::EXPR:
    case Value::Kind::GLOBAL:
    case Value::Kind::CONST: {
      return true;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
static bool IsSingleUse(const Func &func)
{
  unsigned codeUses = 0;
  for (const User *user : func.users()) {
    if (auto *inst = ::cast_or_null<const Inst>(user)) {
      auto *movInst = ::cast<const MovInst>(inst);
      for (const User *movUsers : movInst->users()) {
        codeUses++;
      }
    } else {
      return false;
    }
  }
  return codeUses == 1;
}

// -----------------------------------------------------------------------------
static bool IsCompatible(Type a, Type b)
{
  return a == b
      || (a == Type::I64 && b == Type::V64)
      || (a == Type::V64 && b == Type::I64);
}

// -----------------------------------------------------------------------------
class GlobalForwarder final {
public:
  /// Initialise the analysis.
  GlobalForwarder(Prog &prog, Func &entry);

  /// Simplify loads and build the graph for the reverse transformations.
  bool Forward();
  /// Hoist stores.
  bool Reverse();

private:
  /// Transitive closure of an object.
  struct ObjectClosure {
    /// Set of referenced functions.
    BitSet<Func> Funcs;
    /// Set of referenced objects.
    BitSet<Object> Objects;
  };

  /// Transitive closure of a function.
  struct FuncClosure {
    /// DAG-based representation of the function.
    std::unique_ptr<DAGFunc> DAG;
    /// Set of referenced functions.
    BitSet<Func> Funcs;
    /// Set of escaped objects.
    BitSet<Object> Escaped;
    /// Set of changed objects.
    BitSet<Object> Stored;
    /// Set of dereferenced objects.
    BitSet<Object> Loaded;
    /// Flag to indicate whether any function raises.
    bool Raises;
    /// Flag to indicate whether any function has indirect calls.
    bool Indirect;
  };

  /// Evaluation state of a node.
  struct NodeState {
    /// IDs of referenced functions.
    BitSet<Func> Funcs;
    /// ID of tainted objects.
    BitSet<Object> Escaped;
    /// Set of objects changed to unknown values.
    BitSet<Object> Stored;
    /// Set of dereferenced objects.
    BitSet<Object> Loaded;
    /// Accurate stores.
    std::unordered_map
      < ID<Object>
      , std::map<uint64_t, std::pair<Type, Ref<Inst>>>
      > Stores;

    void Merge(const NodeState &that)
    {
      Funcs.Union(that.Funcs);
      Escaped.Union(that.Escaped);
      Stored.Union(that.Stored);

      auto thisIt = Stores.begin();
      auto thatIt = that.Stores.begin();
      while (thisIt != Stores.end() && thatIt != that.Stores.end()) {
        while (thisIt != Stores.end() && thisIt->first < thatIt->first) {
          Stores.erase(thisIt++);
        }
        if (thisIt == Stores.end()) {
          break;
        }
        while (thatIt != that.Stores.end() && thatIt->first < thisIt->first) {
          ++thatIt;
        }
        if (thatIt == that.Stores.end()) {
          break;
        }
        if (thisIt->first == thatIt->first) {
          if (thisIt->second != thatIt->second) {
            auto &thisMap = thisIt->second;
            auto &thatMap = thatIt->second;
            for (auto it = thisMap.begin(); it != thisMap.end(); ) {
              auto tt = thatMap.find(it->first);
              if (tt == thatMap.end()) {
                thisMap.erase(it++);
                continue;
              }
              if (*it != *tt) {
                thisMap.erase(it++);
              } else {
                ++it;
              }
            }
            if (thisMap.empty()) {
              Stores.erase(thisIt++);
            } else {
              ++thisIt;
            }
          } else {
            ++thisIt;
          }
          ++thatIt;
        }
      }
      Stores.erase(thisIt, Stores.end());
    }

    void Overwrite(const BitSet<Object> &changed)
    {
      Stored.Union(changed);
      for (auto it = Stores.begin(); it != Stores.end(); ) {
        auto id = it->first;
        if (changed.Contains(id) || Escaped.Contains(id)) {
          Stores.erase(it++);
        } else {
          ++it;
        }
      }
    }
  };

  /// Node in the reverse flow graph used to find the earliest
  /// insertion point for stores which can potentially be folded.
  struct ReverseNode {
    /// Predecessor of the node.
    llvm::DenseSet<ReverseNode *> Succs;

    /// Set of stores which can be forwarded here.
    std::unordered_map
      < ID<Object>
      , std::map<uint64_t, std::pair<MemoryStoreInst *, uint64_t>>
      > StorePrecise;
    /// Imprecise, tainted locations.
    BitSet<Object> StoreImprecise;

    /// Set of accurate loads.
    std::unordered_map
      < ID<Object>
      , std::set<std::pair<uint64_t, uint64_t>>
      > LoadPrecise;
    /// Set of inaccurate loads.
    BitSet<Object> LoadImprecise;

    void Merge(const ReverseNode &that)
    {
      for (auto it = StorePrecise.begin(); it != StorePrecise.end(); ) {
        if (that.LoadImprecise.Contains(it->first)) {
          StorePrecise.erase(it++);
          continue;
        }
        auto loadIt = that.LoadPrecise.find(it->first);
        if (loadIt != that.LoadPrecise.end()) {
          llvm_unreachable("not implemented");
        }
        ++it;
      }

      for (auto &[id, stores] : that.StorePrecise) {
        if (LoadImprecise.Contains(id)) {
          continue;
        }
        auto thisLoadIt = LoadPrecise.find(id);
        auto thisStoreIt = StorePrecise.find(id);
        for (auto &[start, storeAndEnd] : stores) {
          auto &[store, end] = storeAndEnd;
          bool killed = false;
          if (!killed && thisLoadIt != LoadPrecise.end()) {
            llvm_unreachable("not implemented");
          }
          if (!killed && thisStoreIt != StorePrecise.end()) {
            for (auto &[thisStart, thisStoreAndEnd] : thisStoreIt->second) {
              auto &[thisStore, thisEnd] = thisStoreAndEnd;
              if (end <= thisStart || thisEnd <= start) {
                continue;
              }
              if (start == thisStart && end == thisEnd) {
                continue;
              }
              llvm_unreachable("not implemented");
            }
          }
          if (!killed) {
            if (thisStoreIt == StorePrecise.end()) {
              StorePrecise[id].emplace(start, storeAndEnd);
            } else {
              thisStoreIt->second.emplace(start, storeAndEnd);
            }
          }
        }
      }

      LoadImprecise.Union(that.LoadImprecise);
      for (const auto &[id, loads] : that.LoadPrecise) {
        LoadPrecise[id].insert(loads.begin(), loads.end());
      }
    }

    /// @section Stores
    ///
    ///
    void Store(ID<Object> id)
    {
      llvm_unreachable("not implemented");
    }

    void Store(
        ID<Object> id,
        uint64_t start,
        uint64_t end,
        MemoryStoreInst *store = nullptr)
    {
      if (StoreImprecise.Contains(id) || LoadImprecise.Contains(id)) {
        return;
      }
      if (auto it = LoadPrecise.find(id); it != LoadPrecise.end()) {
        for (auto [ldStart, ldEnd] : it->second) {
          if (end <= ldStart || ldEnd <= start) {
            continue;
          }
          if (start == ldStart && end == ldEnd) {
            continue;
          }
          llvm_unreachable("not implemented");
        }
      }
      if (auto it = StorePrecise.find(id); it != StorePrecise.end()) {
        for (auto &[stStart, instAndEnd] : it->second) {
          auto &[inst, stEnd] = instAndEnd;
          if (end <= stStart || stEnd <= start) {
            continue;
          } else if (start == stStart && end == stEnd) {
            return;
          } else {
            llvm_unreachable("not implemented");
          }
        }
      }
      StorePrecise[id].emplace(start, std::make_pair(store, end));
    }

    void Store(const BitSet<Object> &stored)
    {
      StoreImprecise.Union(stored);
    }

    /// @section Loads
    ///
    ///
    void Load(ID<Object> id)
    {
      llvm_unreachable("not implemented");
    }

    void Load(ID<Object> id, uint64_t start, uint64_t end)
    {
      auto storeIt = StorePrecise.find(id);
      if (storeIt != StorePrecise.end()) {
        llvm_unreachable("not implemented");
      }
      LoadPrecise[id].emplace(start, end);
    }

    void Load(const BitSet<Object> &loaded)
    {
      for (auto it = LoadPrecise.begin(); it != LoadPrecise.end(); ) {
        if (loaded.Contains(it->first)) {
          LoadPrecise.erase(it++);
        } else {
          ++it;
        }
      }
      LoadImprecise.Union(loaded);
    }

    /// Over-approximates the whole set.
    void Taint(const BitSet<Object> &changed)
    {
      StoreImprecise.Union(changed);
      LoadImprecise.Union(changed);

      for (auto it = LoadPrecise.begin(); it != LoadPrecise.end(); ) {
        if (changed.Contains(it->first)) {
          LoadPrecise.erase(it++);
        } else {
          ++it;
        }
      }
    }
  };

  /// Evaluation state of a function.
  struct FuncState {
    /// Summarised function.
    DAGFunc &DAG;
    /// Current active node.
    unsigned Active;
    /// ID of the node to evaluate accurately.
    unsigned Accurate;
    /// Enumeration of node states.
    std::unordered_map<unsigned, std::unique_ptr<NodeState>> States;

    FuncState(DAGFunc &dag)
      : DAG(dag)
      , Active(dag.rbegin()->Index)
      , Accurate(Active)
    {
    }

    NodeState &GetState(unsigned index)
    {
      auto it = States.emplace(index, nullptr);
      if (it.second) {
        it.first->second.reset(new NodeState());
      }
      return *it.first->second;
    }
  };

  /// Visitor for accurate evaluation.
  class Approximator : public InstVisitor<void> {
  public:
    Approximator(GlobalForwarder &state) : state_(state) {}

    void VisitInst(Inst &inst) override { }

    void VisitMovInst(MovInst &mov) override
    {
      // Record all symbols, except the ones which do not escape.
      state_.Escape(Funcs, Escaped, mov);
    }

    void VisitMemoryStoreInst(MemoryStoreInst &store) override
    {
      // Record a potential non-escaped symbol as mutated.
      if (auto ptr = GetObject(store.GetAddr())) {
        Stored.Insert(state_.GetObjectID(ptr->first));
      }
    }

    void VisitMemoryLoadInst(MemoryLoadInst &load) override
    {
      // Record a potential non-escaped symbol and its closure as mutated.
      if (auto ptr = GetObject(load.GetAddr())) {
        auto id = state_.GetObjectID(ptr->first);
        auto &obj = *state_.objects_[id];
        Funcs.Union(obj.Funcs);
        Loaded.Insert(id);
        Escaped.Union(obj.Objects);
      }
    }

    void VisitCallSite(CallSite &site) override
    {
      if (auto *f = site.GetDirectCallee()) {
        auto &func = *state_.funcs_[state_.GetFuncID(*f)];
        Raises = Raises || func.Raises;
        Indirect = Indirect || func.Indirect;
        Funcs.Union(func.Funcs);
        Escaped.Union(func.Escaped);
        Loaded.Union(func.Loaded);
        Stored.Union(func.Stored);
      } else {
        Indirect = true;
      }
    }

    void VisitTrapInst(TrapInst &) override { }

    void VisitRaiseInst(RaiseInst &raise) override { Raises = true; }

  public:
    /// Flag to set if any node raises.
    bool Raises = false;
    /// Flag to indicate if indirect calls are present.
    bool Indirect = false;
    /// Set of referenced functions.
    BitSet<Func> Funcs;
    /// Set of escaped symbols.
    BitSet<Object> Escaped;
    /// Set of loaded symbols.
    BitSet<Object> Loaded;
    /// Set of stored symbols.
    BitSet<Object> Stored;

  private:
    /// Reference to the transformation.
    GlobalForwarder &state_;
  };

  /// Accurate evaluator which can simplify nodes.
  class Simplifier final : public InstVisitor<bool> {
  public:
    Simplifier(
        GlobalForwarder &state,
        NodeState &node,
        ReverseNode &reverse)
      : state_(state)
      , node_(node)
      , reverse_(reverse)
    {
    }

    bool VisitInst(Inst &inst) override { return false; }

    bool VisitMovInst(MovInst &mov) override
    {
      state_.Escape(node_.Funcs, node_.Escaped, mov);
      return false;
    }

    bool VisitMemoryStoreInst(MemoryStoreInst &store) override
    {
      auto ty = store.GetValue().GetType();
      if (auto ptr = GetObject(store.GetAddr())) {
        auto id = state_.GetObjectID(ptr->first);
        node_.Stored.Insert(id);
        LLVM_DEBUG(llvm::dbgs()
            << "\t\tStore to " << ptr->first->begin()->getName()
            << ", " << id << "\n"
        );
        if (ptr->second) {
          auto off = *ptr->second;
          auto end = off + GetSize(ty);
          if (node_.Escaped.Contains(id)) {
            reverse_.Store(id, off, end);
          } else {
            auto &stores = node_.Stores[id];
            for (auto it = stores.begin(); it != stores.end(); ) {
              auto prevStart = it->first;
              auto prevEnd = prevStart + GetSize(it->second.first);
              if (prevEnd <= off || end <= prevStart) {
                ++it;
              } else {
                stores.erase(it++);
              }
            }
            auto v = store.GetValue();
            LLVM_DEBUG(llvm::dbgs() << "\t\t\tforward " << *v << "\n");
            stores.emplace(*ptr->second, std::make_pair(ty, v));
            reverse_.Store(id, off, end, &store);
          }
        } else {
          llvm_unreachable("not implemented");
        }
      } else {
        node_.Overwrite(node_.Escaped);
        reverse_.Store(node_.Escaped);
      }
      return false;
    }

    bool VisitMemoryLoadInst(MemoryLoadInst &load) override
    {
      if (auto ptr = GetObject(load.GetAddr())) {
        auto id = state_.GetObjectID(ptr->first);
        LLVM_DEBUG(llvm::dbgs()
            << "\t\tLoad from " << ptr->first->begin()->getName()
            << ", " << id << "\n"
        );
        auto &stores = node_.Stores[id];
        if (ptr->second) {
          auto off = *ptr->second;
          auto ty = load.GetType();
          auto end = off + GetSize(ty);
          LLVM_DEBUG(llvm::dbgs()
              << "\t\t\toffset: " << off << ", type: " << ty << "\n"
          );
          // The offset is known - try to record the stored value.
          auto it = stores.find(off);
          if (it != stores.end()) {
            // Forwarding a previous store to load from.
            auto [storeTy, storeValue] = it->second;
            auto storeOrig = storeValue->getParent()->getParent();
            auto loadOrig = load.getParent()->getParent();
            if (IsCompatible(ty, storeTy)) {
              if (storeOrig == loadOrig) {
                ++NumLoadsFolded;
                if (ty == storeTy) {
                  LLVM_DEBUG(llvm::dbgs() << "\t\t\treplace: " << *storeValue << "\n");
                  load.replaceAllUsesWith(storeValue);
                  load.eraseFromParent();
                } else {
                  auto *mov = new MovInst(ty, storeValue, load.GetAnnots());
                  LLVM_DEBUG(llvm::dbgs() << "\t\t\treplace: " << *mov << "\n");
                  load.getParent()->AddInst(mov, &load);
                  load.replaceAllUsesWith(mov);
                  load.eraseFromParent();
                }
                return true;
              } else {
                if (auto mov = ::cast_or_null<MovInst>(storeValue)) {
                  if (IsConstant(mov->GetArg())) {
                    llvm_unreachable("not implemented");
                  } else {
                    // Cannot forward - non-static move.
                    reverse_.Load(id, off, end);
                  }
                } else {
                  // Cannot forward - dynamic value produced in another frame.
                  reverse_.Load(id, off, end);
                }
                return false;
              }
            } else {
              llvm_unreachable("not implemented");
            }
          } else if (!node_.Stored.Contains(id)) {
            // Value not yet mutated, load from static data.
            if (auto *v = state_.Load(ptr->first, off, ty)) {
              ++NumLoadsFolded;
              auto *mov = new MovInst(ty, v, load.GetAnnots());
              LLVM_DEBUG(llvm::dbgs() << "\t\t\treplace: " << *mov << "\n");
              load.getParent()->AddInst(mov, &load);
              load.replaceAllUsesWith(mov);
              load.eraseFromParent();
              return true;
            } else {
              return false;
            }
          } else {
            // De-referencing an object. Add pointees to tainted set.
            auto &obj = *state_.objects_[id];
            node_.Escaped.Union(obj.Objects);
            node_.Funcs.Union(obj.Funcs);
            return false;
          }
        } else {
          llvm_unreachable("not implemented");
        }
      } else {
        // Imprecise load, all pointees should have already been tainted.
        reverse_.Load(node_.Escaped);
        return false;
      }
    }

    bool VisitMemoryExchangeInst(MemoryExchangeInst &xchg) override
    {
      llvm_unreachable("not implemented");
    }

    bool VisitTerminatorInst(TerminatorInst &) override
    {
      llvm_unreachable("cannot evaluate terminator");
    }

  protected:
    /// Reference to the transformation.
    GlobalForwarder &state_;
    /// Reference to the node containing the instruction.
    NodeState &node_;
    /// Reverse node.
    ReverseNode &reverse_;
  };

  /// Approximate the effects of a mov.
  void Escape(BitSet<Func> &funcs, BitSet<Object> &escaped, MovInst &mov);
  /// Approximate the effects of a call.
  void Indirect(
      BitSet<Func> &funcs,
      BitSet<Object> &escaped,
      BitSet<Object> &stored,
      BitSet<Object> &loaded,
      bool &raises
  );
  /// Approximate the effects of a raise.
  void Raise(NodeState &node, ReverseNode &reverse);

  /// Find the atom at an offset in the object.
  std::optional<std::pair<Atom::iterator, int64_t>>
  GetItem(Object *object, uint64_t offset);
  /// Load a value from a memory location.
  Value *Load(Object *object, uint64_t offset, Type type);
  /// Store a value to a memory location.
  bool Store(Object *object, uint64_t offset, Ref<Value> value, Type type);

  /// Return the ID of a function.
  ID<Func> GetFuncID(Func &func)
  {
    auto it = funcToID_.emplace(&func, funcs_.size());
    if (it.second) {
      funcs_.emplace_back(std::make_unique<FuncClosure>());
    }
    return it.first->second;
  }

  /// Return the ID of an object.
  ID<Object> GetObjectID(Object *object)
  {
    auto it = objectToID_.find(object);
    assert(it != objectToID_.end() && "missing object");
    return it->second;
  }

  /// Helper to get the DAG for a function.
  DAGFunc &GetDAG(Func &func)
  {
    auto &dag = funcs_[GetFuncID(func)]->DAG;
    if (!dag) {
      dag.reset(new DAGFunc(func));
    }
    return *dag;
  }

  /// Return a reverse node.
  ReverseNode &GetReverseNode(Func &func, unsigned index)
  {
    std::pair<Func *, unsigned> key(&func, index);
    auto it = reverse_.emplace(key, nullptr);
    if (it.second) {
      it.first->second = std::make_unique<ReverseNode>();
    }
    return *it.first->second;
  }

private:
  /// Analysed program.
  Prog &prog_;
  /// Entry point.
  Func &entry_;

  /// Object to ID.
  std::unordered_map<Object *, ID<Object>> objectToID_;
  /// Mapping from objects to their closures.
  std::vector<std::unique_ptr<ObjectClosure>> objects_;
  /// Mapping from object IDs to objects.
  std::vector<Object *> idToObject_;

  /// Function to ID.
  std::unordered_map<Func *, ID<Func>> funcToID_;
  /// Mapping from functions to their closures.
  std::vector<std::unique_ptr<FuncClosure>> funcs_;

  /// Set of reverse nodes.
  std::unordered_map
    < std::pair<Func *, unsigned>
    , std::unique_ptr<ReverseNode>
    > reverse_;

  /// Evaluation stack.
  std::vector<FuncState> stack_;
};

// -----------------------------------------------------------------------------
GlobalForwarder::GlobalForwarder(Prog &prog, Func &entry)
  : prog_(prog)
  , entry_(entry)
{
  ObjectGraph og(prog);
  for (auto it = llvm::scc_begin(&og); !it.isAtEnd(); ++it) {
    ID<Object> id = objects_.size();
    auto &node = *objects_.emplace_back(std::make_unique<ObjectClosure>());
    for (auto *sccNode : *it) {
      if (auto *obj = sccNode->GetObject()) {
        objectToID_.emplace(obj, id);
      }
    }
    idToObject_.push_back(it->size() == 1 ? (*it)[0]->GetObject() : nullptr);
    for (auto *sccNode : *it) {
      auto *obj = sccNode->GetObject();
      if (!obj) {
        continue;
      }
      for (Atom &atom : *obj) {
        for (Item &item : atom) {
          auto *expr = item.AsExpr();
          if (!expr) {
            continue;
          }
          switch (expr->GetKind()) {
            case Expr::Kind::SYMBOL_OFFSET: {
              auto *g = static_cast<SymbolOffsetExpr *>(expr)->GetSymbol();
              switch (g->GetKind()) {
                case Global::Kind::FUNC: {
                  auto &func = static_cast<Func &>(*g);
                  node.Funcs.Insert(GetFuncID(func));
                  continue;
                }
                case Global::Kind::ATOM: {
                  auto *object = static_cast<Atom &>(*g).getParent();
                  node.Objects.Insert(GetObjectID(object));
                  continue;
                }
                case Global::Kind::BLOCK:
                case Global::Kind::EXTERN: {
                  // Blocks and externs are not recorded.
                  continue;
                }
              }
              llvm_unreachable("invalid global kind");
            }
          }
          llvm_unreachable("invalid expression kind");
        }
      }
    }
  }

  CallGraph cg(prog);
  ReferenceGraph rg(prog, cg);
  for (Func &func : prog) {
    auto &rgNode = rg[func];

    auto it = funcToID_.emplace(&func, funcs_.size());
    if (it.second) {
      funcs_.emplace_back(std::make_unique<FuncClosure>());
    }
    auto &node = *funcs_[it.first->second];
    node.Raises = rgNode.HasRaise;
    node.Indirect = rgNode.HasIndirectCalls;

    for (auto *read : rgNode.Read) {
      // Entire transitive closure is loaded, only pointees escape.
      auto id = GetObjectID(read);
      auto &obj = *objects_[id];
      node.Funcs.Union(obj.Funcs);
      node.Escaped.Union(obj.Objects);
      node.Loaded.Union(obj.Objects);
      node.Loaded.Insert(id);
    }
    for (auto *written : rgNode.Written) {
      // The specific item is changed.
      node.Stored.Insert(GetObjectID(written));
    }
    for (auto *g : rgNode.Escapes) {
      switch (g->GetKind()) {
        case Global::Kind::FUNC: {
          auto &func = static_cast<Func &>(*g);
          node.Funcs.Insert(GetFuncID(func));
          continue;
        }
        case Global::Kind::ATOM: {
          auto *object = static_cast<Atom &>(*g).getParent();
          auto id = GetObjectID(object);
          auto &obj = *objects_[id];
          // Transitive closure is fully tainted.
          node.Funcs.Union(obj.Funcs);
          node.Escaped.Union(obj.Objects);
          node.Escaped.Insert(id);
          node.Loaded.Union(obj.Objects);
          node.Loaded.Insert(id);
          node.Stored.Union(obj.Objects);
          node.Stored.Insert(id);
          continue;
        }
        case Global::Kind::BLOCK:
        case Global::Kind::EXTERN: {
          // Blocks and externs are not recorded.
          continue;
        }
      }
      llvm_unreachable("invalid global kind");
    }
  }
}

// -----------------------------------------------------------------------------
bool GlobalForwarder::Forward()
{
  bool changed = false;
  stack_.emplace_back(GetDAG(entry_));
  while (!stack_.empty()) {
    auto &state = *stack_.rbegin();
    auto &func = state.DAG.GetFunc();
    auto active = state.Active;
    auto &dag = *state.DAG[active];
    auto &node = state.GetState(active);
    auto &reverse = GetReverseNode(func, active);

    LLVM_DEBUG(llvm::dbgs()
        << "===================\n"
        << active << ":" << dag << " in "
        << state.DAG.GetFunc().getName() << "\n"
    );

    auto &preds = dag.Preds;
    for (auto it = preds.begin(); it != preds.end(); ++it) {
      auto *pred = *it;
      LLVM_DEBUG(llvm::dbgs() << "\tpred: " << *pred << "\n");
      auto st = state.States.find(pred->Index);
      assert(st != state.States.end() && "missing predecessor");

      if (it == preds.begin()) {
        node = *st->second;
      } else {
        node.Merge(*st->second);
      }

      unsigned minSucc = std::numeric_limits<unsigned>::max();
      for (auto succ : state.DAG[pred->Index]->Succs) {
        minSucc = std::min(minSucc, succ->Index);
      }
      if (minSucc == active && !pred->IsExit()) {
        state.States.erase(pred->Index);
      }
      GetReverseNode(func, pred->Index).Succs.insert(&reverse);
    }

    LLVM_DEBUG(llvm::dbgs()
        << "===================\n"
        << "\tStored: " << node.Stored << "\n"
        << "\tEscaped: " << node.Escaped << "\n"
    );

    bool accurate = false;
    if (state.Accurate == active) {
      accurate = true;
      if (!dag.Succs.empty()) {
        state.Accurate = (*dag.Succs.begin())->Index;
        LLVM_DEBUG(llvm::dbgs() << "\tNext: " << state.Accurate << "\n");
      }
    }

    bool returns = false;
    if (dag.IsLoop) {
      LLVM_DEBUG(llvm::dbgs() << "\tApproximating " << dag << "\n");
      Approximator a(*this);
      for (auto *block : dag.Blocks) {
        for (auto &inst : *block) {
          a.Dispatch(inst);
        }
      }
      if (a.Indirect) {
        Indirect(a.Funcs, a.Escaped, a.Stored, a.Loaded, a.Raises);
      }

      node.Escaped.Union(a.Escaped);
      node.Loaded.Union(a.Loaded);
      node.Overwrite(a.Stored);
      node.Overwrite(a.Escaped);

      reverse.Taint(node.Escaped);
      reverse.Taint(node.Loaded);
      reverse.Taint(node.Stored);

      if (a.Raises) {
        Raise(node, reverse);
      }
    } else {
      assert(dag.Blocks.size() == 1 && "invalid block");
      auto *block = *dag.Blocks.begin();
      LLVM_DEBUG(llvm::dbgs() << "\tEvaluating " << block->getName() << "\n");

      for (auto it = block->begin(); std::next(it) != block->end(); ) {
        auto &inst = *it++;
        LLVM_DEBUG(llvm::dbgs() << "\t" << inst << "\n");
        changed = Simplifier(*this, node, reverse).Dispatch(inst) || changed;
      }

      if (auto *call = ::cast_or_null<CallSite>(block->GetTerminator())) {
        LLVM_DEBUG(llvm::dbgs() << "\t" << *call << "\n");
        auto *f = call->GetDirectCallee();
        if (accurate && f && IsSingleUse(*f)) {
          auto &calleeDAG = GetDAG(*f);
          auto &calleeState = stack_.emplace_back(calleeDAG);
          calleeState.GetState(calleeState.Active) = node;
          reverse.Succs.insert(&GetReverseNode(*f, calleeDAG.rbegin()->Index));
          continue;
        } else {
          bool raises = false;
          bool indirect = false;
          BitSet<Object> stored;
          BitSet<Object> loaded;

          if (f) {
            auto &calleeNode = *funcs_[GetFuncID(*f)];
            node.Funcs.Union(calleeNode.Funcs);
            node.Escaped.Union(calleeNode.Escaped);
            loaded = calleeNode.Loaded;
            raises = calleeNode.Raises;
            indirect = calleeNode.Indirect;
            stored = calleeNode.Stored;
          } else {
            indirect = true;
          }
          if (indirect) {
            Indirect(node.Funcs, node.Escaped, stored, loaded, raises);
          }
          node.Loaded.Union(loaded);
          node.Overwrite(stored);
          node.Overwrite(node.Escaped);

          reverse.Taint(node.Escaped);
          reverse.Load(loaded);
          reverse.Store(stored);

          if (raises) {
            if (auto *invoke = ::cast_or_null<InvokeInst>(call)) {
              auto throwIndex = state.DAG[invoke->GetThrow()]->Index;
              auto &throwNode = state.GetState(throwIndex);
              throwNode.Merge(node);
              auto &throwReverse = GetReverseNode(func, throwIndex);
              reverse.Succs.insert(&throwReverse);
            } else {
              Raise(node, reverse);
            }
          }
        }
      }
    }

    #ifndef NDEBUG
    LLVM_DEBUG(llvm::dbgs() << "===================\n");
    LLVM_DEBUG(llvm::dbgs() << "\tLoad: " << reverse.LoadImprecise << "\n");
    for (auto &[id, loads] : reverse.LoadPrecise) {
      for (auto &[start, end] : loads) {
        LLVM_DEBUG(llvm::dbgs()
            << "\t\t" << id << " + " << start << "," << end << "\t"
        );
      }
    }
    LLVM_DEBUG(llvm::dbgs() << "\tStore: " << reverse.StoreImprecise << "\n");
    for (auto &[id, stores] : reverse.StorePrecise) {
      for (auto &[off, storeAndEnd] : stores) {
        auto &[store, end] = storeAndEnd;
        LLVM_DEBUG(llvm::dbgs()
            << "\t\t" << id << " + " << off << "," << end << "\n"
        );
      }
    }
    #endif

    if (active == 0) {
      if (stack_.size() <= 1) {
        stack_.pop_back();
      } else {
        LLVM_DEBUG(llvm::dbgs()
          << "===================\n"
          << "Returning\n"
        );
        FuncState &calleeState = *stack_.rbegin();

        // Collect information from all returning nodes.
        std::optional<NodeState> retState;
        for (auto *node : calleeState.DAG) {
          if (node->IsReturn) {
            LLVM_DEBUG(llvm::dbgs() << "\t" << *node << "\n");
            auto st = state.States.find(node->Index);
            assert(st != state.States.end() && "missing predecessor");
            if (retState) {
              retState->Merge(*st->second);
            } else {
              retState.emplace(std::move(*st->second));
            }
          }
        }

        for (;;) {
          stack_.pop_back();
          FuncState &callerState = *stack_.rbegin();
          auto retActive = callerState.Active;
          auto &dag = *callerState.DAG[retActive];
          assert(dag.Blocks.size() == 1 && "invalid block");
          auto *site = ::cast<CallSite>((*dag.Blocks.begin())->GetTerminator());
          switch (site->GetKind()) {
            default: llvm_unreachable("not a call");
            case Inst::Kind::TAIL_CALL: {
              if (stack_.size() > 1) {
                continue;
              }
              stack_.clear();
              break;
            }
            case Inst::Kind::INVOKE:
            case Inst::Kind::CALL: {
              LLVM_DEBUG(llvm::dbgs() << "\t" << retActive << " " << dag << "\n");
              if (retState) {
                callerState.GetState(callerState.Active).Merge(*retState);
              }
              callerState.Active--;
              break;
            }
          }
          break;
        }
      }
    } else {
      state.Active--;
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool GlobalForwarder::Reverse()
{
  std::unordered_set<ReverseNode *> visited;
  std::function<void(ReverseNode *node)> dfs =
    [&, this] (ReverseNode *node)
    {
      if (!visited.insert(node).second) {
        return;
      }
      // Compute information for all the successors in the DAG.
      for (auto *succ : node->Succs) {
        dfs(succ);
      }
      // Merge information from successors.
      std::optional<ReverseNode> merged;
      for (auto *succ : node->Succs) {
        if (merged) {
          merged->Merge(*succ);
        } else {
          merged = *succ;
        }
      }
      // Apply the transfer function.
      if (merged) {
        for (auto &&[id, stores] : merged->StorePrecise) {
          if (node->StoreImprecise.Contains(id)) {
            continue;
          }
          bool killed = false;
          auto storeIt = node->StorePrecise.find(id);
          if (!killed && storeIt != node->StorePrecise.end()) {
            for (auto &[start, instAndEnd] : stores) {
              auto &[inst, end] = instAndEnd;
              for (auto &[nodeStart, nodeInstAndEnd] : storeIt->second) {
                auto &[nodeInst, nodeEnd] = nodeInstAndEnd;
                if (end <= nodeStart || nodeEnd <= start) {
                  continue;
                }
                if (start == nodeStart && end == nodeEnd) {
                  killed = true;
                  continue;
                }
                llvm_unreachable("not implemented");
              }
            }
          }
          auto loadIt = node->LoadPrecise.find(id);
          if (!killed && loadIt != node->LoadPrecise.end()) {
            llvm_unreachable("not implemented");
          }
          if (!killed) {
            node->StorePrecise.emplace(id, std::move(stores));
          }
        }
        for (auto &[id, loads] : merged->LoadPrecise) {
          if (node->LoadImprecise.Contains(id)) {
            continue;
          }

          auto storeIt = node->StorePrecise.find(id);
          for (auto [ldStart, ldEnd] : loads) {
            bool killed = false;
            if (storeIt != node->StorePrecise.end()) {
              for (auto &[nodeStart, nodeInstAndEnd] : storeIt->second) {
                auto &[nodeInst, nodeEnd] = nodeInstAndEnd;
                if (ldEnd <= nodeStart || nodeEnd <= ldStart) {
                  continue;
                }
                if (ldStart == nodeStart && ldEnd == nodeEnd) {
                  killed = true;
                  continue;
                }
                llvm_unreachable("not implemented");
              }
            }
            if (!killed) {
              node->LoadPrecise[id].emplace(ldStart, ldEnd);
            }
          }
        }
        node->LoadImprecise.Union(merged->LoadImprecise);
      }
    };

  auto *entry = &GetReverseNode(entry_, GetDAG(entry_).rbegin()->Index);
  dfs(entry);

  bool changed = false;
  for (auto &[id, stores] : entry->StorePrecise) {
    auto *object = idToObject_[id];
    if (!object) {
      continue;
    }

    for (auto &[off, instAndEnd] : stores) {
      auto &[store, end] = instAndEnd;
      if (!store) {
        continue;
      }
      auto mov = ::cast_or_null<MovInst>(store->GetValue());
      if (!mov || !IsConstant(mov->GetArg())) {
        continue;
      }
      if (Store(object, off, mov->GetArg(), mov.GetType())) {
        LLVM_DEBUG(llvm::dbgs() << "Folded store: " << *store << "\n");
        store->eraseFromParent();
        NumStoresFolded++;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
void GlobalForwarder::Escape(
    BitSet<Func> &funcs,
    BitSet<Object> &escaped,
    MovInst &mov)
{
  auto g = ::cast_or_null<Global>(mov.GetArg());
  if (!g) {
    return;
  }

  bool escapes = false;
  if (g->IsLocal()) {
    for (User *user : mov.users()) {
      if (auto *store = ::cast_or_null<MemoryStoreInst>(user)) {
        if (store->GetValue() == mov.GetSubValue(0)) {
          escapes = true;
          break;
        }
        continue;
      }
      if (auto *load = ::cast_or_null<MemoryLoadInst>(user)) {
        continue;
      }
      if (auto *call = ::cast_or_null<CallSite>(user)) {
        for (auto arg : call->args()) {
          if (arg == mov.GetSubValue(0)) {
            escapes = true;
            break;
          }
        }
        if (escapes) {
          break;
        }
        continue;
      }
      escapes = true;
      break;
    }
  } else {
    escapes = true;
  }

  if (escapes) {
    switch (g->GetKind()) {
      case Global::Kind::FUNC: {
        auto id = GetFuncID(*::cast<Func>(g));
        LLVM_DEBUG(llvm::dbgs()
          << "\t\tEscape: " << g->getName() << " as " << id << "\n"
        );
        funcs.Insert(id);
        return;
      }
      case Global::Kind::ATOM: {
        auto id = GetObjectID(::cast<Atom>(g)->getParent());
        LLVM_DEBUG(llvm::dbgs()
          << "\t\tEscape: " << g->getName() << " as " << id << "\n"
        );
        auto &obj = *objects_[id];
        funcs.Union(obj.Funcs);
        escaped.Union(obj.Objects);
        escaped.Insert(id);
        return;
      }
      case Global::Kind::BLOCK:
      case Global::Kind::EXTERN: {
        return;
      }
    }
    llvm_unreachable("invalid global kind");
  }
}

// -----------------------------------------------------------------------------
void GlobalForwarder::Indirect(
    BitSet<Func> &funcs,
    BitSet<Object> &escaped,
    BitSet<Object> &stored,
    BitSet<Object> &loaded,
    bool &raise)
{
  std::queue<ID<Func>> q;
  for (auto f : funcs) {
    q.push(f);
  }

  while (!q.empty()) {
    auto id = q.front();
    q.pop();

    auto &func = *funcs_[id];
    for (auto id : func.Funcs - funcs) {
      q.push(id);
    }

    funcs.Union(func.Funcs);
    escaped.Union(func.Escaped);
    stored.Union(func.Stored);
    loaded.Union(func.Loaded);
    raise = raise || func.Raises;
  }
}

// -----------------------------------------------------------------------------
void GlobalForwarder::Raise(NodeState &node, ReverseNode &reverse)
{
  assert(!stack_.empty() && "empty call stack");
  for (auto it = std::next(stack_.rbegin()); it != stack_.rend(); ++it) {
    auto &dag = it->DAG;
    auto &dagNode = *dag[it->Active];
    auto &func = dag.GetFunc();
    assert(dagNode.Blocks.size() == 1 && "invalid block");
    auto *call = ::cast<CallSite>((*dagNode.Blocks.begin())->GetTerminator());
    switch (call->GetKind()) {
      default: llvm_unreachable("not a call");
      case Inst::Kind::INVOKE: {
        auto *invoke = static_cast<InvokeInst *>(call);
        auto throwIndex = it->DAG[invoke->GetThrow()]->Index;
        auto &throwState = it->GetState(throwIndex);
        throwState.Merge(node);
        auto &throwReverse = GetReverseNode(func, throwIndex);
        reverse.Succs.insert(&throwReverse);
        return;
      }
      case Inst::Kind::TAIL_CALL:
      case Inst::Kind::CALL: {
        continue;
      }
    }
  }
}

// -----------------------------------------------------------------------------
static Value *LoadInt(Atom::iterator it, unsigned off, unsigned size)
{
  switch (it->GetKind()) {
    case Item::Kind::INT8: {
      if (size == 1) {
        return new ConstantInt(it->GetInt8());
      }
      break;
    }
    case Item::Kind::INT16: {
      if (size == 2) {
        return new ConstantInt(it->GetInt16());
      }
      break;
    }
    case Item::Kind::INT32: {
      if (size == 4) {
        return new ConstantInt(it->GetInt32());
      }
      break;
    }
    case Item::Kind::INT64: {
      if (size == 8) {
        return new ConstantInt(it->GetInt64());
      }
      break;
    }
    case Item::Kind::STRING: {
      if (size == 1) {
        return new ConstantInt(it->getString()[off]);
      }
      break;
    }
    case Item::Kind::SPACE: {
      if (off + size <= it->GetSpace()) {
        return new ConstantInt(0);
      }
      break;
    }
    case Item::Kind::FLOAT64: {
      break;
    }
    case Item::Kind::EXPR: {
      auto *expr = it->GetExpr();
      switch (expr->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto *sym = static_cast<SymbolOffsetExpr *>(expr);
          if (size == 8) {
            if (sym->GetOffset()) {
              return expr;
            } else {
              return sym->GetSymbol();
            }
          }
          break;
        }
      }
      return nullptr;
    }
  }
  // TODO: conversion based on endianness.
  return nullptr;
}

// -----------------------------------------------------------------------------
std::optional<std::pair<Atom::iterator, int64_t>>
GlobalForwarder::GetItem(Object *object, uint64_t offset)
{
  auto *data = object->getParent();
  auto *atom = &*object->begin();

  uint64_t i;
  uint64_t itemOff;
  auto it = atom->begin();
  for (i = 0; it != atom->end() && i + it->GetSize() <= offset; ++it) {
    if (it == atom->end()) {
      // TODO: jump to next atom.
      return std::nullopt;
    }
    i += it->GetSize();
  }
  if (it == atom->end()) {
    return std::nullopt;
  }

  itemOff = offset - i;
  return std::make_pair(it, itemOff);
}

// -----------------------------------------------------------------------------
Value *GlobalForwarder::Load(Object *object, uint64_t offset, Type type)
{
  auto it = GetItem(object, offset);
  if (!it) {
    return nullptr;
  }

  switch (type) {
    case Type::I8: {
      return LoadInt(it->first, it->second, 1);
    }
    case Type::I16: {
      return LoadInt(it->first, it->second, 2);
    }
    case Type::I32: {
      return LoadInt(it->first, it->second, 4);
    }
    case Type::I64:
    case Type::V64: {
      return LoadInt(it->first, it->second, 8);
    }
    case Type::F32:
    case Type::F64:
    case Type::I128:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
static bool StoreExpr(Atom::iterator it, unsigned off, Expr *expr)
{
  switch (it->GetKind()) {
    case Item::Kind::INT8: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT16: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT32: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::EXPR:
    case Item::Kind::INT64:
    case Item::Kind::FLOAT64: {
      auto *item = new Item(expr);
      it->getParent()->AddItem(item, &*it);
      it->eraseFromParent();
      return true;
    }
    case Item::Kind::STRING: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::SPACE: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid item kind");
}

// -----------------------------------------------------------------------------
bool GlobalForwarder::Store(
    Object *object,
    uint64_t offset,
    Ref<Value> value,
    Type type)
{
  auto it = GetItem(object, offset);
  if (!it) {
    return false;
  }

  switch (value->GetKind()) {
    case Value::Kind::INST: {
      llvm_unreachable("not a constant");
    }
    case Value::Kind::GLOBAL: {
      auto *g = &*::cast<Global>(value);
      return StoreExpr(it->first, it->second, SymbolOffsetExpr::Create(g, 0));
    }
    case Value::Kind::EXPR: {
      return StoreExpr(it->first, it->second, &*::cast<Expr>(value));
    }
    case Value::Kind::CONST: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
const char *GlobalForwardPass::kPassID = "global-forward";

// -----------------------------------------------------------------------------
const char *GlobalForwardPass::GetPassName() const
{
  return "Global Load/Store Forwarding";
}

// -----------------------------------------------------------------------------
bool GlobalForwardPass::Run(Prog &prog)
{
  auto &cfg = GetConfig();
  const std::string start = cfg.Entry.empty() ? "_start" : cfg.Entry;
  auto *entry = ::cast_or_null<Func>(prog.GetGlobal(start));
  if (!entry) {
    return false;
  }

  GlobalForwarder forwarder(prog, *entry);

  bool changed = false;
  changed = forwarder.Forward() || changed;
  changed = forwarder.Reverse() || changed;
  return changed;
}
