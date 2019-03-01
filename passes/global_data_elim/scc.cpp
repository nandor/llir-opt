// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/global_data_elim/node.h"
#include "passes/global_data_elim/scc.h"



// -----------------------------------------------------------------------------
SCCSolver::SCCSolver()
{
}

// -----------------------------------------------------------------------------
SCCSolver &SCCSolver::Full(SetIter begin, SetIter end)
{
  // Find SCCs rooted at unvisited nodes.
  index_ = 1ull;
  for (auto it = begin; it != end; ++it) {
    if (*it) {
      auto *node = it->get();
      if (node->Index == 0) {
        Traverse(node);
      }

      if (auto *deref = node->Deref()) {
        if (deref->Index == 0){
          Traverse(deref);
        }
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
    for (auto *v : set->set_outs()) {
      if (v->Index == 0) {
        Traverse(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (v->OnStack) {
        node->Link = std::min(node->Link, v->Link);
      }
    }

    for (auto *v : set->deref_outs()) {
      if (v->Index == 0) {
        Traverse(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (v->OnStack) {
        node->Link = std::min(node->Link, v->Link);
      }
    }
  }

  if (auto *deref = node->AsDeref()) {
    for (auto *v : deref->set_outs()) {
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
