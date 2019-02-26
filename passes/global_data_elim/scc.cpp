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
void SCCSolver::Solve(
    NodeIter begin,
    NodeIter end,
    std::function<void(const Group &)> &&f)
{
  f_ = f;

  // Reset the traversal metadata.
  for (auto it = begin; it != end; ++it) {
    auto *node = it->get();
    node->Index = 0;
    node->Link = 0;
    node->OnStack = false;
  }

  // Find SCCs rooted at unvisited nodes.
  index_ = 1ull;
  for (auto it = begin; it != end; ++it) {
    auto *node = it->get();
    if (node->Index == 0) {
      Connect(node);
    }
  }

  // Stack must be empty by this point.
  assert(stack_.size() == 0);

  // Traverse the computed SCCs.
  for (auto &scc : sccs_) {
    f_(scc);
  }
}

// -----------------------------------------------------------------------------
void SCCSolver::Connect(GraphNode *node)
{
  node->Index = index_;
  node->Link = index_;
  index_ += 1;
  stack_.push(node);
  node->OnStack = true;

  if (auto *set = node->AsSet()) {
    for (auto *v : set->set_outs()) {
      if (v->Index == 0) {
        Connect(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (v->OnStack) {
        node->Link = std::min(node->Link, v->Link);
      }
    }

    for (auto *v : set->deref_outs()) {
      if (v->Index == 0) {
        Connect(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (v->OnStack) {
        node->Link = std::min(node->Link, v->Link);
      }
    }
  }

  if (auto *deref = node->AsSet()) {
    for (auto *v : deref->set_outs()) {
      if (v->Index == 0) {
        Connect(v);
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
