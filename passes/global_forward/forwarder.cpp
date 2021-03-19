// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>
#include <queue>
#include <limits>
#include <unordered_set>


#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/SCCIterator.h>
#include <llvm/Support/Debug.h>

#include "core/insts.h"
#include "core/expr.h"
#include "core/analysis/call_graph.h"
#include "core/analysis/object_graph.h"
#include "core/analysis/reference_graph.h"
#include "passes/global_forward/forwarder.h"

#define DEBUG_TYPE "global-forward"

STATISTIC(NumStoresFolded, "Stores folded");
STATISTIC(NumStoresKilled, "Dead stores removed");


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
GlobalForwarder::GlobalForwarder(Prog &prog, Func &entry)
  : prog_(prog)
  , entry_(entry)
{
  ObjectGraph og(prog);
  CallGraph cg(prog);
  ReferenceGraph rg(prog, cg);

  for (auto it = llvm::scc_begin(&cg); !it.isAtEnd(); ++it) {
    // Create a node for the entire SCC.
    ID<Func> id = funcs_.size();
    funcs_.emplace_back(std::make_unique<FuncClosure>());
    for (auto *funcNode : *it) {
      auto *func = funcNode->GetCaller();
      if (!func) {
        continue;
      }
      funcToID_.emplace(func, id);
    }
  }

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

  for (auto it = llvm::scc_begin(&cg); !it.isAtEnd(); ++it) {
    for (auto *funcNode : *it) {
      auto *func = funcNode->GetCaller();
      if (!func) {
        continue;
      }
      auto &node = *funcs_[GetFuncID(*func)];
      auto &rgNode = rg[*func];
      node.Raises = rgNode.HasRaise;
      node.Indirect = rgNode.HasIndirectCalls;

      for (auto *read : rgNode.ReadRanges) {
        // Entire transitive closure is loaded, only pointees escape.
        auto objectID = GetObjectID(read);
        auto &obj = *objects_[objectID];
        node.Funcs.Union(obj.Funcs);
        node.Escaped.Union(obj.Objects);
        node.Loaded.Insert(objectID);
      }
      for (auto &[read, offsets] : rgNode.ReadOffsets) {
        // Entire transitive closure is loaded, only pointees escape.
        auto objectID = GetObjectID(read);
        auto &obj = *objects_[objectID];
        node.Funcs.Union(obj.Funcs);
        node.Escaped.Union(obj.Objects);
        node.Loaded.Insert(objectID);
      }
      for (auto *written : rgNode.WrittenRanges) {
        // The specific item is changed.
        node.Stored.Insert(GetObjectID(written));
      }
      for (auto &[written, offsets] : rgNode.WrittenOffsets) {
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
            auto objectID = GetObjectID(object);
            auto &obj = *objects_[objectID];
            // Transitive closure is fully tainted.
            node.Funcs.Union(obj.Funcs);
            node.Escaped.Union(obj.Objects);
            node.Escaped.Insert(objectID);
            node.Loaded.Union(obj.Objects);
            node.Loaded.Insert(objectID);
            node.Stored.Union(obj.Objects);
            node.Stored.Insert(objectID);
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

    LLVM_DEBUG(llvm::dbgs() << "===================\n");
    LLVM_DEBUG(node.dump(llvm::dbgs()));

    bool accurate = false;
    if (state.Accurate == active) {
      accurate = true;
      if (!dag.Succs.empty()) {
        state.Accurate = (*dag.Succs.begin())->Index;
        LLVM_DEBUG(llvm::dbgs() << "\tNext: " << state.Accurate << "\n");
      }
    }

    if (dag.IsLoop) {
      LLVM_DEBUG(llvm::dbgs() << "\tApproximating " << dag << "\n");
      Approximator a(*this);
      for (auto *block : dag.Blocks) {
        for (auto &inst : *block) {
          a.Dispatch(inst);
        }
      }
      if (a.Indirect) {
        Indirect(
            a.Funcs,
            a.Escaped,
            a.Stored,
            a.Loaded,
            a.Raises
        );
      }

      node.Funcs.Union(a.Funcs);
      node.Escaped.Union(a.Escaped);
      node.Overwrite(a.Stored | a.Escaped);

      reverse.Load(a.Loaded | node.Escaped);
      reverse.Store(a.Stored | node.Escaped);

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

      auto *term = block->GetTerminator();
      switch (term->GetKind()) {
        default: break;
        // Enter or approximate callee.
        case Inst::Kind::CALL:
        case Inst::Kind::TAIL_CALL:
        case Inst::Kind::INVOKE: {
          auto *call = ::cast_or_null<CallSite>(term);
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
              stored = calleeNode.Stored;
              raises = calleeNode.Raises;
              indirect = calleeNode.Indirect;
            } else {
              indirect = true;
            }
            if (indirect) {
              Indirect(
                  node.Funcs,
                  node.Escaped,
                  stored,
                  loaded,
                  raises
              );
            }
            node.Overwrite(stored | node.Escaped);
            reverse.Load(loaded | node.Escaped);
            reverse.Store(stored | node.Escaped);

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
          break;
        }
        // Approximate raise.
        case Inst::Kind::RAISE: {
          Raise(node, reverse);
          break;
        }
      }
    }

    #ifndef NDEBUG
    LLVM_DEBUG(llvm::dbgs() << "===================\n");
    LLVM_DEBUG(reverse.dump(llvm::dbgs()));
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
  std::vector<ReverseNodeState *> nodes;
  std::unordered_set<ReverseNodeState *> visited;
  std::function<void(ReverseNodeState *node)> dfs =
    [&, this] (ReverseNodeState *node)
    {
      if (!visited.insert(node).second) {
        return;
      }
      // Compute information for all the successors in the DAG.
      for (auto *succ : node->Succs) {
        dfs(succ);
      }
      nodes.push_back(node);
    };

  auto *entry = &GetReverseNode(entry_, GetDAG(entry_).rbegin()->Index);
  dfs(entry);

  LLVM_DEBUG(llvm::dbgs() << "===================\n");
  LLVM_DEBUG(llvm::dbgs() << "Reverse:\n");
  LLVM_DEBUG(llvm::dbgs() << "===================\n");

  bool changed = false;
  for (ReverseNodeState *node : nodes) {
    LLVM_DEBUG(llvm::dbgs() << "===================\n");
    LLVM_DEBUG(llvm::dbgs() << node->Node << "\n");
    LLVM_DEBUG(llvm::dbgs() << "===================\n");
    // Merge information from successors.
    std::optional<ReverseNodeState> merged;
    LLVM_DEBUG(llvm::dbgs() << "Merged:\n");
    for (auto *succ : node->Succs) {
      LLVM_DEBUG(llvm::dbgs() << "\t" << succ->Node << "\n");
      if (merged) {
        merged->Merge(*succ);
      } else {
        merged.emplace(*succ);
      }
    }
    #ifndef DEBUG
    if (merged) {
      LLVM_DEBUG(merged->dump(llvm::dbgs()));
      LLVM_DEBUG(node->dump(llvm::dbgs()));
    }
    #endif
    // Apply the transfer function.
    if (merged) {
      auto stores = node->Stores;
      for (auto &&[id, stores] : merged->Stores) {
        if (node->Stored.Contains(id)) {
          continue;
        }
        auto storeIt = node->Stores.find(id);
        for (auto &[start, instAndEnd] : stores) {
          auto &[inst, end] = instAndEnd;

          bool killed = false;
          if (storeIt != node->Stores.end()) {
            for (auto &[nodeStart, nodeInstAndEnd] : storeIt->second) {
              auto &[nodeInst, nodeEnd] = nodeInstAndEnd;
              if (end <= nodeStart || nodeEnd <= start) {
                continue;
              }
              if (start == nodeStart && end == nodeEnd) {
                killed = true;
                break;
              }
              llvm_unreachable("not implemented");
            }
          }
          if (!killed) {
            node->Stores[id].emplace(start, instAndEnd);
          }
        }
      }

      node->Loaded.Union(merged->Loaded);
    }
    LLVM_DEBUG(llvm::dbgs() << "Final:\n");
    #ifndef DEBUG
    LLVM_DEBUG(node->dump(llvm::dbgs()));
    #endif
  };

  for (auto &[id, stores] : entry->Stores) {
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
      if (!mov || !mov->GetArg()->IsConstant()) {
        continue;
      }
      if (object->Store(off, mov->GetArg(), mov.GetType())) {
        LLVM_DEBUG(llvm::dbgs() << "Folded store: " << *store->GetAddr() << "\n");
        store->eraseFromParent();
        NumStoresFolded++;
        changed = true;
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
  Ref<Global> g;
  auto arg = mov.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::CONST:
    case Value::Kind::INST: {
      return;
    }
    case Value::Kind::GLOBAL: {
      g = ::cast<Global>(arg);
      break;
    }
    case Value::Kind::EXPR: {
      g = ::cast<SymbolOffsetExpr>(arg)->GetSymbol();
      break;
    }
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
        auto &obj = *objects_[id];
        LLVM_DEBUG(llvm::dbgs()
            << "\t\tEscape: " << g->getName() << " as " << id
            << ", " << obj.Funcs
            << ", " << obj.Objects
            << "\n"
        );
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
  LLVM_DEBUG(llvm::dbgs()
      << "Indirect:\n"
      << "\tfuncs: " << funcs << "\n"
      << "\tescaped: " << escaped << "\n"
      << "\tstored: " << stored << "\n"
      << "\tloaded: " << loaded << "\n"
  );
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
void GlobalForwarder::Raise(NodeState &node, ReverseNodeState &reverse)
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
