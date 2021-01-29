// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>
#include <limits>
#include <queue>

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

  /// Simplify functions along the path of interest.
  bool Run();

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
    /// Set of tainted objects.
    BitSet<Object> Tainted;
    /// Set of changed objects.
    BitSet<Object> Changed;
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
    BitSet<Object> Tainted;
    /// Set of objects changed to unknown values.
    BitSet<Object> Changed;
    /// Accurate stores.
    std::unordered_map
      < ID<Object>
      , std::map<uint64_t, std::pair<Type, Ref<Inst>>>
      > Stores;

    void Merge(const NodeState &that)
    {
      Funcs.Union(that.Funcs);
      Tainted.Union(that.Tainted);
      Changed.Union(that.Changed);

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

    void Change(const BitSet<Object> &changed)
    {
      Changed.Union(changed);
      for (auto it = Stores.begin(); it != Stores.end(); ) {
        auto id = it->first;
        if (changed.Contains(id) || Tainted.Contains(id)) {
          Stores.erase(it++);
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
    /// Set up the evaluator.
    Approximator(
        GlobalForwarder &state,
        bool &raises,
        bool &indirect,
        NodeState &node)
      : state_(state)
      , raises_(raises)
      , indirect_(indirect)
      , node_(node)
    {
    }

    /// Nothing to do.
    void VisitInst(Inst &inst) override { }

    void VisitMovInst(MovInst &mov) override
    {
      state_.Taint(node_, mov);
    }

    void VisitMemoryStoreInst(MemoryStoreInst &store) override
    {
      if (auto ptr = GetObject(store.GetAddr())) {
        auto id = state_.GetObjectID(ptr->first);
        node_.Stores.erase(id);
        node_.Changed.Insert(id);
      } else {
        node_.Changed.Union(node_.Tainted);
      }
    }

    void VisitMemoryLoadInst(MemoryLoadInst &load) override
    {
      if (auto ptr = GetObject(load.GetAddr())) {
        auto id = state_.GetObjectID(ptr->first);
        auto it = node_.Stores.find(id);
        if (it != node_.Stores.end()) {
          // Load is precise, does not introduce any imprecision.
        } else if (!node_.Changed.Contains(id)) {
          // Loaded location was not yet tainted.
        } else {
          // Taint with whatever the object points to.
          auto &obj = *state_.objects_[id];
          node_.Funcs.Union(obj.Funcs);
          node_.Tainted.Union(obj.Objects);
        }
      }
    }

    void VisitCallSite(CallSite &site) override
    {
      if (auto *f = site.GetDirectCallee()) {
        auto &func = *state_.funcs_[state_.GetFuncID(*f)];
        raises_ = raises_ || func.Raises;
        indirect_ = indirect_ || func.Indirect;
        node_.Funcs.Union(func.Funcs);
        node_.Tainted.Union(func.Tainted);
        node_.Change(func.Changed);
      } else {
        indirect_ = true;
      }
    }

    void VisitTrapInst(TrapInst &) override { }

    void VisitRaiseInst(RaiseInst &raise) override { raises_ = true; }

  protected:
    /// Reference to the transformation.
    GlobalForwarder &state_;
    /// Flag to set if any node raises.
    bool &raises_;
    /// Flag to indicate if indirect calls are present.
    bool &indirect_;
    /// Reference to the node containing the instruction.
    NodeState &node_;
  };

  /// Accurate evaluator which can simplify nodes.
  class Simplifier final : public InstVisitor<bool> {
  public:
    Simplifier(GlobalForwarder &state, NodeState &node)
      : state_(state)
      , node_(node)
    {
    }

    /// Nothing to do.
    bool VisitInst(Inst &inst) override { return false; }

    bool VisitMovInst(MovInst &mov) override
    {
      state_.Taint(node_, mov);
      return false;
    }

    bool VisitMemoryStoreInst(MemoryStoreInst &store) override
    {
      if (auto ptr = GetObject(store.GetAddr())) {
        auto id = state_.GetObjectID(ptr->first);
        node_.Changed.Insert(id);
        LLVM_DEBUG(llvm::dbgs()
            << "\t\tStore to " << ptr->first->begin()->getName()
            << ", " << id << "\n"
        );
        if (!node_.Tainted.Contains(id)) {
          if (ptr->second) {
            auto &stores = node_.Stores[id];
            auto ty = store.GetValue().GetType();
            auto stStart = *ptr->second;
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
            LLVM_DEBUG(llvm::dbgs() << "\t\t\tforward\n");
            stores.emplace(
                *ptr->second,
                std::make_pair(ty, store.GetValue())
            );
          } else {
            llvm_unreachable("not implemented");
          }
        }
      } else {
        node_.Changed.Union(node_.Tainted);
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
                if (ty == storeTy) {
                  LLVM_DEBUG(llvm::dbgs() << "\t\t\treplace: " << *storeValue << "\n");
                  load.replaceAllUsesWith(storeValue);
                } else {
                  auto *mov = new MovInst(ty, storeValue, load.GetAnnots());
                  LLVM_DEBUG(llvm::dbgs() << "\t\t\treplace: " << *mov << "\n");
                  load.getParent()->AddInst(mov, &load);
                  load.replaceAllUsesWith(mov);
                }
                load.eraseFromParent();
                return true;
              } else {
                if (auto mov = ::cast_or_null<MovInst>(storeValue)) {
                  if (IsConstant(mov->GetArg())) {
                    llvm_unreachable("not implemented");
                  } else {
                    // Cannot forward - non-static move.
                  }
                } else {
                  // Cannot forward - dynamic value produced in another frame.
                }
              }
            } else {
              llvm_unreachable("not implemented");
            }
          } else if (!node_.Changed.Contains(id)) {
            // Value not yet mutated, load from static data.
            if (auto *v = state_.Load(ptr->first, offset, ty)) {
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
            node_.Tainted.Union(obj.Objects);
            node_.Funcs.Union(obj.Funcs);
          }
        } else {
          llvm_unreachable("not implemented");
        }
      } else {
        // Imprecise load, all pointees should have already been tainted.
      }
      return false;
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
  };

  /// Approximate the effects of a mov.
  void Taint(NodeState &node, MovInst &mov);
  /// Approximate the effects of a call.
  BitSet<Object> Indirect(NodeState &node, bool &raises);
  /// Approximate the effects of a raise.
  void Raise(NodeState &node);
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
      auto *obj = sccNode->GetObject();
      if (!obj) {
        continue;
      }

      objectToID_.emplace(obj, id);

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
      auto &obj = *objects_[GetObjectID(read)];
      node.Funcs.Union(obj.Funcs);
      node.Tainted.Union(obj.Objects);
    }
    for (auto *written : rgNode.Written) {
      node.Changed.Insert(GetObjectID(written));
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
          auto objectID = GetObjectID(object);
          auto &obj = *objects_[objectID];
          node.Funcs.Union(obj.Funcs);
          node.Tainted.Union(obj.Objects);
          node.Tainted.Insert(objectID);
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
bool GlobalForwarder::Run()
{
  bool changed = false;
  stack_.emplace_back(GetDAG(entry_));
  while (!stack_.empty()) {
    auto &state = *stack_.rbegin();
    auto active = state.Active;
    auto &nodeDAG = *state.DAG[active];
    auto &nodeState = state.GetState(active);

    LLVM_DEBUG(llvm::dbgs()
        << "===================\n"
        << active << ":" << nodeDAG << " in "
        << state.DAG.GetFunc().getName() << "\n"
    );

    auto &preds = nodeDAG.Preds;
    for (auto it = preds.begin(); it != preds.end(); ++it) {
      auto *pred = *it;
      LLVM_DEBUG(llvm::dbgs() << "\tpred: " << *pred << "\n");
      auto st = state.States.find(pred->Index);
      assert(st != state.States.end() && "missing predecessor");

      if (it == preds.begin()) {
        nodeState = *st->second;
      } else {
        nodeState.Merge(*st->second);
      }

      unsigned minSucc = std::numeric_limits<unsigned>::max();
      for (auto succ : state.DAG[pred->Index]->Succs) {
        minSucc = std::min(minSucc, succ->Index);
      }
      if (minSucc == active && !pred->IsExit()) {
        state.States.erase(pred->Index);
      }
    }

    LLVM_DEBUG(llvm::dbgs()
        << "===================\n"
        << "\tChanged: " << nodeState.Changed << "\n"
        << "\tTainted: " << nodeState.Tainted << "\n"
    );

    bool accurate = false;
    if (state.Accurate == active) {
      accurate = true;
      if (!nodeDAG.Succs.empty()) {
        state.Accurate = (*nodeDAG.Succs.begin())->Index;
        LLVM_DEBUG(llvm::dbgs() << "\tNext: " << state.Accurate << "\n");
      }
    }

    bool returns = false;
    if (nodeDAG.IsLoop || !accurate) {
      LLVM_DEBUG(llvm::dbgs() << "\tApproximating " << nodeDAG << "\n");
      bool raises = false;
      bool indirect = false;
      for (auto *block : nodeDAG.Blocks) {
        for (auto &inst : *block) {
          Approximator(*this, raises, indirect, nodeState).Dispatch(inst);
        }
      }
      if (indirect) {
        nodeState.Change(Indirect(nodeState, raises));
      }
      if (raises) {
        Raise(nodeState);
      }
    } else {
      assert(nodeDAG.Blocks.size() == 1 && "invalid block");
      auto *block = *nodeDAG.Blocks.begin();
      LLVM_DEBUG(llvm::dbgs() << "\tEvaluating " << block->getName() << "\n");

      for (auto it = block->begin(); std::next(it) != block->end(); ) {
        auto &inst = *it++;
        LLVM_DEBUG(llvm::dbgs() << "\t" << inst << "\n");
        changed = Simplifier(*this, nodeState).Dispatch(inst) || changed;
      }

      if (auto *call = ::cast_or_null<CallSite>(block->GetTerminator())) {
        LLVM_DEBUG(llvm::dbgs() << "\t" << *call << "\n");
        if (auto *f = call->GetDirectCallee(); f && IsSingleUse(*f)) {
          auto &calleeState = stack_.emplace_back(GetDAG(*f));
          calleeState.GetState(calleeState.Active) = nodeState;
          continue;
        } else {
          auto &func = *funcs_[GetFuncID(*f)];
          nodeState.Funcs.Union(func.Funcs);
          nodeState.Tainted.Union(func.Tainted);
          nodeState.Change(func.Changed);
          bool raises = func.Raises;
          if (!f || func.Indirect) {
            Indirect(nodeState, raises);
          }
          if (raises) {
            if (auto *invoke = ::cast_or_null<InvokeInst>(call)) {
              auto throwIndex = state.DAG[invoke->GetThrow()]->Index;
              auto &throwState = state.GetState(throwIndex);
              throwState.Merge(nodeState);
            } else {
              Raise(nodeState);
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
        NodeState retState;
        for (auto *node : calleeState.DAG) {
          if (node->IsReturn) {
            LLVM_DEBUG(llvm::dbgs() << "\t" << *node << "\n");
            auto st = state.States.find(node->Index);
            assert(st != state.States.end() && "missing predecessor");
            retState.Merge(*st->second);
          }
        }

        for (;;) {
          stack_.pop_back();
          FuncState &callerState = *stack_.rbegin();
          auto &dag = *callerState.DAG[callerState.Active];
          assert(dag.Blocks.size() == 1 && "invalid block");
          auto *site = ::cast<CallSite>((*dag.Blocks.begin())->GetTerminator());
          switch (site->GetKind()) {
            default: llvm_unreachable("not a call");
            case Inst::Kind::TAIL_CALL: {
              llvm_unreachable("not implemented");
            }
            case Inst::Kind::INVOKE: {
              auto *invoke = static_cast<CallInst *>(site);
              auto retIndex = callerState.DAG[invoke->GetCont()]->Index;
              auto &state = callerState.GetState(retIndex);
              state.Merge(retState);
              callerState.Active--;
              break;
            }
            case Inst::Kind::CALL: {
              auto *call = static_cast<CallInst *>(site);
              auto retIndex = callerState.DAG[call->GetCont()]->Index;
              auto &state = callerState.GetState(retIndex);
              state.Merge(retState);
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
void GlobalForwarder::Taint(NodeState &node, MovInst &mov)
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
        node.Funcs.Insert(id);
        return;
      }
      case Global::Kind::ATOM: {
        auto id = GetObjectID(::cast<Atom>(g)->getParent());
        LLVM_DEBUG(llvm::dbgs()
          << "\t\tEscape: " << g->getName() << " as " << id << "\n"
        );
        auto &obj = *objects_[id];
        node.Funcs.Union(obj.Funcs);
        node.Tainted.Union(obj.Objects);
        node.Tainted.Insert(id);
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
BitSet<Object> GlobalForwarder::Indirect(NodeState &node, bool &raise)
{
  std::queue<ID<Func>> q;
  for (auto f : node.Funcs) {
    q.push(f);
  }

  BitSet<Object> changed;
  while (!q.empty()) {
    auto id = q.front();
    q.pop();

    auto &func = *funcs_[id];
    for (auto id : func.Funcs - node.Funcs) {
      q.push(id);
    }

    node.Funcs.Union(func.Funcs);
    node.Tainted.Union(func.Tainted);
    changed.Union(func.Changed);
    raise = raise || func.Raises;
  }
  return changed;
}

// -----------------------------------------------------------------------------
void GlobalForwarder::Raise(NodeState &node)
{
  assert(!stack_.empty() && "empty call stack");
  for (auto it = std::next(stack_.rbegin()); it != stack_.rend(); ++it) {
    auto &dag = *it->DAG[it->Active];
    assert(dag.Blocks.size() == 1 && "invalid block");
    auto *call = ::cast<CallSite>((*dag.Blocks.begin())->GetTerminator());
    switch (call->GetKind()) {
      default: llvm_unreachable("not a call");
      case Inst::Kind::TAIL_CALL: {
        llvm_unreachable("not implemented");
      }
      case Inst::Kind::INVOKE: {
        auto *invoke = static_cast<InvokeInst *>(call);
        auto throwIndex = it->DAG[invoke->GetThrow()]->Index;
        auto &throwState = it->GetState(throwIndex);
        throwState.Merge(node);
        return;
      }
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
        llvm_unreachable("not implemented");
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
        char chr = it->getString()[off];
        llvm_unreachable("not implemented");
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
  return GlobalForwarder(prog, *entry).Run();
}
