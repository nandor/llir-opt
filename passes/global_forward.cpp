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
      , std::map<uint64_t, MemoryStoreInst *>
      > Stores;
    /// Set of accurate loads.
    std::unordered_map<ID<Object>, uint64_t> LoadPrecise;
    /// Set of inaccurate loads.
    BitSet<Object> LoadImprecise;
    /// Set of killed stores.
    BitSet<Object> Killed;

    void Merge(const ReverseNode &that)
    {
      //llvm_unreachable("not implemented");
    }

    void Store(ID<Object> id)
    {
      //llvm_unreachable("not implemented");
    }

    void Store(ID<Object> id, uint64_t off)
    {
    }

    void Store(ID<Object> id, uint64_t off, MemoryStoreInst &store)
    {
      //llvm_unreachable("not implemented");
    }

    void Store(const BitSet<Object> &stored)
    {
      //llvm_unreachable("not implemented");
    }

    void Load(ID<Object> id)
    {
      //llvm_unreachable("not implemented");
    }

    void Load(ID<Object> id, uint64_t off)
    {
      //llvm_unreachable("not implemented");
    }

    void Load(const BitSet<Object> &loaded)
    {
      //llvm_unreachable("not implemented");
    }

    void Taint(const BitSet<Object> &changed)
    {
      Load(changed);
      Store(changed);
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
      if (auto ptr = GetObject(store.GetAddr())) {
        auto id = state_.GetObjectID(ptr->first);
        node_.Stored.Insert(id);
        LLVM_DEBUG(llvm::dbgs()
            << "\t\tStore to " << ptr->first->begin()->getName()
            << ", " << id << "\n"
        );
        if (ptr->second) {
          auto off = *ptr->second;
          if (node_.Escaped.Contains(id)) {
            reverse_.Store(id, off);
          } else {
            auto &stores = node_.Stores[id];
            auto ty = store.GetValue().GetType();
            auto stStart = off;
            auto stEnd = stStart + GetSize(ty);
            for (auto it = stores.begin(); it != stores.end(); ) {
              auto prevStart = it->first;
              auto prevEnd = prevStart + GetSize(it->second.first);
              if (prevEnd <= stStart || stEnd <= prevStart) {
                ++it;
              } else {
                stores.erase(it++);
              }
            }
            auto v = store.GetValue();
            LLVM_DEBUG(llvm::dbgs() << "\t\t\tforward " << *v << "\n");
            stores.emplace(*ptr->second, std::make_pair(ty, v));
            reverse_.Store(id, off, store);
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
          auto offset = *ptr->second;
          auto ty = load.GetType();
          LLVM_DEBUG(llvm::dbgs()
              << "\t\t\toffset: " << offset << ", type: " << ty << "\n"
          );
          // The offset is known - try to record the stored value.
          auto it = stores.find(offset);
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
                    reverse_.Load(id, *ptr->second);
                  }
                } else {
                  // Cannot forward - dynamic value produced in another frame.
                  reverse_.Load(id, *ptr->second);
                }
                return false;
              }
            } else {
              llvm_unreachable("not implemented");
            }
          } else if (!node_.Stored.Contains(id)) {
            // Value not yet mutated, load from static data.
            if (auto *v = state_.Load(ptr->first, offset, ty)) {
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

  /// Load a value from a memory location.
  Value *Load(Object *object, uint64_t offset, Type type);

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
          auto &calleeNode = *funcs_[GetFuncID(*f)];
          node.Funcs.Union(calleeNode.Funcs);
          node.Escaped.Union(calleeNode.Escaped);
          node.Loaded.Union(calleeNode.Loaded);

          bool raises = calleeNode.Raises;
          BitSet<Object> stored(calleeNode.Stored);
          if (!f || calleeNode.Indirect) {
            Indirect(node.Funcs, node.Escaped, node.Loaded, stored, raises);
          }
          node.Overwrite(stored);
          node.Overwrite(node.Escaped);

          reverse.Taint(node.Escaped);
          reverse.Taint(node.Loaded);
          reverse.Taint(node.Stored);

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
        //llvm_unreachable("not implemented");
      } else {
        //llvm_unreachable("not implemented");
      }
    };

  auto *entry = &GetReverseNode(entry_, GetDAG(entry_).rbegin()->Index);
  dfs(entry);

  bool changed = false;
  for (auto &[id, stores] : entry->Stores) {
    //llvm_unreachable("not implemented");
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
Value *GlobalForwarder::Load(Object *object, uint64_t offset, Type type)
{
  auto *data = object->getParent();
  auto *atom = &*object->begin();

  uint64_t i;
  uint64_t itemOff;
  auto it = atom->begin();
  for (i = 0; it != atom->end() && i + it->GetSize() <= offset; ++it) {
    if (it == atom->end()) {
      // TODO: jump to next atom.
      return nullptr;
    }
    i += it->GetSize();
  }
  if (it == atom->end()) {
    return nullptr;
  }
  itemOff = offset - i;

  switch (type) {
    case Type::I8: {
      return LoadInt(it, itemOff, 1);
    }
    case Type::I16: {
      return LoadInt(it, itemOff, 2);
    }
    case Type::I32: {
      return LoadInt(it, itemOff, 4);
    }
    case Type::I64:
    case Type::V64: {
      return LoadInt(it, itemOff, 8);
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
