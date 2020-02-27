// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_map>

#include <llvm/Support/raw_ostream.h>

#include "core/adt/bitset.h"
#include "core/adt/hash.h"
#include "core/atom.h"
#include "core/block.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts_call.h"
#include "passes/pta/node.h"
#include "passes/pta/solver.h"



// -----------------------------------------------------------------------------
ConstraintSolver::ConstraintSolver()
  : scc_(&graph_)
{
  extern_ = Root();
  Subset(Load(extern_), extern_);
}

// -----------------------------------------------------------------------------
ConstraintSolver::~ConstraintSolver()
{
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
    auto *contents = deref->Contents();
    if (auto *deref = contents->Deref()) {
      return deref;
    } else {
      return Deref(contents);
    }
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
Func * ConstraintSolver::Map(const ID<Func *> &id)
{
  return idToFunc_[id];
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
Extern * ConstraintSolver::Map(const ID<Extern *> &id)
{
  return idToExt_[id];
}

// -----------------------------------------------------------------------------
SetNode *ConstraintSolver::Map(const ID<SetNode *> &id)
{
  return graph_.Find(id);
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
        from->points_to_node([this, &deref](auto id) {
          auto *v = graph_.Find(id);
          deref->set_ins([v, this](auto storeID) {
            auto *store = graph_.Find(storeID);
            if (store->AddSet(v)) {
              queue_.Push(store->GetID());
            }
            return store->GetID();
          });
          deref->set_outs([v, this](auto loadID) {
            auto *load = graph_.Find(loadID);
            if (v->AddSet(load)) {
              queue_.Push(v->GetID());
            }
            return load->GetID();
          });
          return v->GetID();
        });
      }

      // Propagate values from the node to outgoing nodes. If the node is a
      // candidate for SCC collapsing, remove it later. Collapsed node IDs
      // are also removed after the traversal to simplify the graph.
      {
        bool collapse = false;

        from->sets([&collapse, &visited, from, this](auto toID) {
          auto *to = graph_.Find(toID);
          if (to->GetID() == from->GetID()) {
            return to->GetID();
          }

          if (from->Equals(to) && visited.insert({ from, to }).second) {
            collapse = true;
          }

          if (from->Propagate(to)) {
            queue_.Push(to->GetID());
          }
          return to->GetID();
        });

        if (collapse) {
          scc_.Single(from).Solve([this](auto &group) {
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
