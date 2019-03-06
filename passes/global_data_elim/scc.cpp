// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/global_data_elim/node.h"
#include "passes/global_data_elim/scc.h"



// -----------------------------------------------------------------------------
SCCSolver::SCCSolver(
    const std::vector<std::unique_ptr<SetNode>> &sets,
    const std::vector<std::unique_ptr<DerefNode>> &derefs)
  : sets_(sets)
  , derefs_(derefs)
  , epoch_(1ull)
{
}

// -----------------------------------------------------------------------------
SCCSolver &SCCSolver::Full()
{
  // Reset the traversal.
  epoch_ += 1;
  index_ = 1;

  // Find SCCs rooted at unvisited nodes.
  for (auto &set : sets_) {
    if (set && set->Epoch != epoch_) {
      Traverse(set.get());
    }
  }
  for (auto &deref : derefs_) {
    if (deref && deref->Epoch != epoch_){
      Traverse(deref.get());
    }
  }

  // Stack must be empty by this point.
  assert(stack_.size() == 0);
  return *this;
}

// -----------------------------------------------------------------------------
SCCSolver &SCCSolver::Single(GraphNode *node)
{
  // Reset the traversal.
  epoch_ += 1;
  index_ = 1ull;

  // Find SCCs starting at this node.
  Traverse(node);

  // Stack must be empty after traversal.
  assert(stack_.size() == 0);
  return *this;
}

// -----------------------------------------------------------------------------
void SCCSolver::Solve(std::function<void(const Group &)> &&f)
{
  // Traverse the computed SCCs.
  for (auto &scc : sccs_) {
    if (scc.size() > 1) {
      f(scc);
    }
  }
  sccs_.clear();
}

// -----------------------------------------------------------------------------
void SCCSolver::Traverse(GraphNode *node)
{
  node->Epoch = epoch_;
  node->Index = index_;
  node->Link = index_;
  index_ += 1;
  stack_.push(node);
  node->InComponent = false;

  if (auto *set = node->AsSet()) {
    for (auto id : set->set_outs()) {
      auto *v = sets_.at(id).get();
      if (v->Epoch != epoch_) {
        Traverse(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (!v->InComponent) {
        node->Link = std::min(node->Link, v->Link);
      }
    }

    for (auto id : set->deref_outs()) {
      auto *v = derefs_.at(id).get();
      if (v->Epoch != epoch_) {
        Traverse(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (!v->InComponent) {
        node->Link = std::min(node->Link, v->Link);
      }
    }
  }

  if (auto *deref = node->AsDeref()) {
    for (auto id : deref->set_outs()) {
      auto *v = sets_.at(id).get();
      if (v->Epoch != epoch_) {
        Traverse(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (!v->InComponent) {
        node->Link = std::min(node->Link, v->Link);
      }
    }
  }

  if (node->Link == node->Index) {
    auto &scc = sccs_.emplace_back();
    GraphNode *v;
    do {
      v = stack_.top();
      stack_.pop();
      v->InComponent = true;
      scc.push_back(v);
    } while (v != node);
  }
}
