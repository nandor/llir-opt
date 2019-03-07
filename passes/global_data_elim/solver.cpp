// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_map>

#include <llvm/Support/raw_ostream.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/func.h"
#include "core/hash.h"
#include "core/inst.h"
#include "core/insts_call.h"
#include "passes/global_data_elim/bitset.h"
#include "passes/global_data_elim/node.h"
#include "passes/global_data_elim/solver.h"



// -----------------------------------------------------------------------------
ConstraintSolver::ConstraintSolver()
  : scc_(&graph_)
{
  extern_ = Root();
  Subset(Load(extern_), extern_);
}

// -----------------------------------------------------------------------------
SetNode *ConstraintSolver::Set()
{
  auto *set = graph_.Set();
  return set;
}

// -----------------------------------------------------------------------------
DerefNode *ConstraintSolver::Deref(SetNode *set)
{
  queue_.Push(set->GetID());
  return graph_.Deref(set);
}

// -----------------------------------------------------------------------------
Node *ConstraintSolver::Load(Node *ptr)
{
  auto *node = ptr->ToGraph();
  if (auto *set = node->AsSet()) {
    if (auto *deref = set->Deref()) {
      return deref;
    } else {
      return Deref(set);
    }
  }
  if (auto *deref = node->AsDeref()) {
    return Deref(deref->Contents());
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Subset(Node *from, Node *to)
{
  auto *nodeFrom = from->ToGraph();
  auto *nodeTo = to->ToGraph();
  if (auto *setFrom = nodeFrom->AsSet()) {
    queue_.Push(setFrom->GetID());
    if (auto *setTo = nodeTo->AsSet()) {
      setFrom->AddSet(setTo);
    }
    if (auto *derefTo = nodeTo->AsDeref()) {
      queue_.Push(derefTo->Node()->GetID());
      setFrom->AddDeref(derefTo);
    }
  }
  if (auto *derefFrom = nodeFrom->AsDeref()) {
    queue_.Push(derefFrom->Node()->GetID());
    if (auto *setTo = nodeTo->AsSet()) {
      derefFrom->AddSet(setTo);
    }
    if (auto *derefTo = nodeTo->AsDeref()) {
      queue_.Push(derefTo->Node()->GetID());
      derefFrom->Contents()->AddDeref(derefTo);
    }
  }
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root()
{
  auto *set = Set();
  return Root(set);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(Func *func)
{
  auto *set = Set();
  set->AddFunc(Map(func));
  return Root(set);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(Extern *ext)
{
  auto *set = Set();
  set->AddExtern(Map(ext));
  return Root(set);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(RootNode *node)
{
  auto *set = Set();
  set->AddNode(node->Set()->GetID());
  return Root(set);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(SetNode *set)
{
  return graph_.Root(set);
}

// -----------------------------------------------------------------------------
ID<Func *> ConstraintSolver::Map(Func *func)
{
  auto it = funcToID_.emplace(func, idToFunc_.size());
  if (it.second) {
    idToFunc_.push_back(func);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
ID<Extern *> ConstraintSolver::Map(Extern *ext)
{
  auto it = extToID_.emplace(ext, idToExt_.size());
  if (it.second) {
    idToExt_.push_back(ext);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Anchor(Node *node)
{
  if (node) {
    if (auto *root = node->AsRoot()) {
      return root;
    } else {
      auto *graph = node->ToGraph();
      if (auto *set = graph->AsSet()) {
        return Root(set);
      }
      if (auto *deref = graph->AsDeref()) {
        return Root(deref->Contents());
      }
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

// -----------------------------------------------------------------------------
Node *ConstraintSolver::Empty()
{
  return Set();
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Chunk(Atom *atom, RootNode *root)
{
  atoms_.emplace(atom, root);
  return root;
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Call(
    const std::vector<Inst *> &context,
    Node *callee,
    const std::vector<Node *> &args)
{
  auto *ret = Root();

  std::vector<RootNode *> argsRoot;
  for (auto *node : args) {
    argsRoot.push_back(Anchor(node));
  }

  calls_.emplace_back(context, Anchor(callee), argsRoot, ret);
  return ret;
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Solve()
{
  std::unordered_map<DerefNode *, uint32_t> collapse;

  // Simplify the graph, coalescing strongly connected components.
  scc_
    .Full()
    .Solve([&collapse, this](auto &group) {
      SetNode *united = nullptr;
      for (auto &node : group) {
        if (auto *set = node->AsSet()) {
          united = graph_.Union(united, set);
        }
      }
      for (auto &node : group) {
        if (auto *deref = node->AsDeref()) {
          collapse[deref] = united->GetID();
        }
      }
      queue_.Push(united->GetID());
    });

  // Find edges to propagate values along.
  std::unordered_set<std::pair<Node *, Node *>> visited;

  while (!queue_.Empty()) {
    if (auto *from = graph_.Get(queue_.Pop())) {
      // HCD is implemented here - the points-to set of the node is unified
      // with the collapse node. Special handling is required for the source
      // node, as the node cannot be modified while iterating over its pts set.
      if (auto *deref = from->Deref()) {
        auto it = collapse.find(deref);
        if (it != collapse.end()) {
          auto *united = graph_.Find(it->second);

          bool mergeFrom = false;
          if (united == from) {
            mergeFrom = true;
            united = nullptr;
          }

          for (auto id : from->points_to_node()) {
            auto *v = graph_.Find(id);
            if (v == from) {
              mergeFrom = true;
            } else {
              united = graph_.Union(united, v);
            }
          }

          if (mergeFrom) {
            auto fromID = from->GetID();
            united = graph_.Union(united, from);
            if (united->GetID() != fromID) {
              queue_.Push(united->GetID());
              continue;
            }
          }
        }

        // Add edges from nodes which load/store from a pointer.
        // Points-To Sets are also compacted here, crucial for performance.
        std::vector<std::pair<uint32_t, uint32_t>> fixups;
        for (auto id : from->points_to_node()) {
          auto *v = graph_.Find(id);
          if (v->GetID() != id) {
            fixups.emplace_back(id, v->GetID());
          }
          for (auto storeID : deref->set_ins()) {
            auto *store = graph_.Find(storeID);
            if (store->AddSet(v)) {
              queue_.Push(store->GetID());
            }
          }
          for (auto loadID : deref->set_outs()) {
            auto *load = graph_.Find(loadID);
            if (v->AddSet(load)) {
              queue_.Push(v->GetID());
            }
          }
        }

        for (const auto &fixup : fixups) {
          from->UpdateNode(fixup.first, fixup.second);
        }
      }

      // Propagate values from the node to outgoing nodes. If the node is a
      // candidate for SCC collapsing, remove it later. Collapsed node IDs
      // are also removed after the traversal to simplify the graph.
      bool collapse = false;
      {
        std::vector<std::pair<uint32_t, uint32_t>> fixups;

        for (auto toID : from->sets()) {
          auto *to = graph_.Find(toID);
          if (to->GetID() == from->GetID()) {
            continue;
          }

          if (from->Equals(to) && visited.insert({ from, to }).second) {
            collapse = true;
          }

          if (from->Propagate(to)) {
            queue_.Push(to->GetID());
          }

          if (to->GetID() != toID) {
            fixups.emplace_back(toID, to->GetID());
          }
        }

        for (const auto &fixup : fixups) {
          from->UpdateSet(fixup.first, fixup.second);
        }
      }

      if (collapse) {
        scc_
          .Single(from)
          .Solve([this](auto &group) {
            SetNode *united = nullptr;
            for (auto &node : group) {
              if (auto *set = node->AsSet()) {
                united = graph_.Union(united, set);
              }
            }
            queue_.Push(united->GetID());
          });
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::vector<std::pair<std::vector<Inst *>, Func *>> ConstraintSolver::Expand()
{
  Solve();

  std::vector<std::pair<std::vector<Inst *>, Func *>> callees;
  for (auto &call : calls_) {
    for (auto id : call.Callee->Set()->points_to_func()) {
      auto *func = idToFunc_[id];

      // Only expand each call site once.
      if (!call.Expanded.insert(func).second) {
        continue;
      }

      // Call to be expanded, with context.
      callees.emplace_back(call.Context, func);

      // Connect arguments and return value.
      auto &funcSet = this->Lookup(call.Context, func);
      for (unsigned i = 0; i < call.Args.size(); ++i) {
        if (auto *arg = call.Args[i]) {
          if (i >= funcSet.Args.size()) {
            if (func->IsVarArg()) {
              Subset(arg, funcSet.VA);
            }
          } else {
            Subset(arg, funcSet.Args[i]);
          }
        }
      }
      Subset(funcSet.Return, call.Return);
    }

    for (auto id : call.Callee->Set()->points_to_ext()) {
      assert(!"not implemented");
    }
  }

  return callees;
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Lookup(Global *g)
{
  auto it = globals_.emplace(g, nullptr);
  if (it.second) {
    switch (g->GetKind()) {
      case Global::Kind::SYMBOL: {
        break;
      }
      case Global::Kind::EXTERN: {
        it.first->second = Root(static_cast<Extern *>(g));
        break;
      }
      case Global::Kind::FUNC: {
        it.first->second = Root(static_cast<Func *>(g));
        break;
      }
      case Global::Kind::BLOCK: {
        break;
      }
      case Global::Kind::ATOM: {
        it.first->second = Root(atoms_[static_cast<Atom *>(g)]);
        break;
      }
    }
  }
  return it.first->second;

}

// -----------------------------------------------------------------------------
ConstraintSolver::FuncSet &ConstraintSolver::Lookup(
    const std::vector<Inst *> &calls,
    Func *func)
{
  auto key = func;
  auto it = funcs_.emplace(key, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<FuncSet>();
    auto f = it.first->second.get();
    f->Return = Root();
    f->VA = Root();
    f->Frame = Root();
    for (auto &arg : func->params()) {
      f->Args.push_back(Root());
    }
    f->Expanded = false;
  }
  return *it.first->second;
}
