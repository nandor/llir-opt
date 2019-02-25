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

  for (auto it = begin; it != end; ++it) {
    auto *node = it->get();
    node->Index = 0;
    node->Link = 0;
    node->OnStack = false;
  }

  index_ = 1ull;
  for (auto it = begin; it != end; ++it) {
    auto *node = it->get();
    if (node->Index == 0) {
      Connect(node);
    }
  }
}

// -----------------------------------------------------------------------------
void SCCSolver::Connect(Node *node)
{
  node->Index = index_;
  node->Link = index_;
  index_ += 1;
  stack_.push(node);
  node->OnStack = true;

  for (auto *v : node->outs()) {
    if (v->Index == 0) {
      Connect(v);
      node->Link = std::min(node->Link, v->Link);
    } else if (v->OnStack) {
      node->Link = std::min(node->Link, v->Link);
    }
  }

  if (node->Link == node->Index) {
    std::vector<Node *> scc;
    Node *v;
    do {
      v = stack_.top();
      stack_.pop();
      v->OnStack = false;
      scc.push_back(v);
    } while (v != node);
    f_(scc);
  }
}
