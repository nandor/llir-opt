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
{
}

// -----------------------------------------------------------------------------
SCCSolver &SCCSolver::Full()
{
  // Find SCCs rooted at unvisited nodes.
  index_ = 1ull;
  for (auto &set : sets_) {
    if (set) {
      if (set->Index == 0) {
        Traverse(set.get());
      }
    }
  }
  for (auto &deref : derefs_) {
    if (deref) {
      if (deref->Index == 0){
        Traverse(deref.get());
      }
    }
  }

  // Stack must be empty by this point.
  assert(stack_.size() == 0);
  return *this;
}

// -----------------------------------------------------------------------------
SCCSolver &SCCSolver::Single(GraphNode *node)
{
  Traverse(node);
  assert(stack_.size() == 0);
  return *this;
}

// -----------------------------------------------------------------------------
void SCCSolver::Solve(std::function<void(const Group &)> &&f)
{
  // Traverse the computed SCCs.
  for (auto &scc : sccs_) {
    for (auto *node : scc) {
      node->Index = 0;
      node->Link = 0;
      node->OnStack = false;
    }
    f(scc);
  }
}

// -----------------------------------------------------------------------------
void SCCSolver::Traverse(GraphNode *node)
{
  node->Index = index_;
  node->Link = index_;
  index_ += 1;
  stack_.push(node);
  node->OnStack = true;

  if (auto *set = node->AsSet()) {
    for (auto id : set->set_outs()) {
      auto *v = sets_.at(id).get();
      if (v->Index == 0) {
        Traverse(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (v->OnStack) {
        node->Link = std::min(node->Link, v->Link);
      }
    }

    for (auto id : set->deref_outs()) {
      auto *v = derefs_.at(id).get();
      if (v->Index == 0) {
        Traverse(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (v->OnStack) {
        node->Link = std::min(node->Link, v->Link);
      }
    }
  }

  if (auto *deref = node->AsDeref()) {
    for (auto id : deref->set_outs()) {
      auto *v = sets_.at(id).get();
      if (v->Index == 0) {
        Traverse(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (v->OnStack) {
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
      v->OnStack = false;
      scc.push_back(v);
    } while (v != node);
  }
}
