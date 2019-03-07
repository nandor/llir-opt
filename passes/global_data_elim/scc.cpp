// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/global_data_elim/node.h"
#include "passes/global_data_elim/scc.h"
#include "passes/global_data_elim/solver.h"



// -----------------------------------------------------------------------------
SCCSolver::SCCSolver(ConstraintSolver *solver)
  : solver_(solver)
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
  for (auto &set : solver_->sets_) {
    if (set && set->Epoch != epoch_) {
      VisitFull(set);
    }
  }
  for (auto &deref : solver_->derefs_) {
    if (deref && deref->Epoch != epoch_){
      VisitFull(deref);
    }
  }

  // Stack must be empty by this point.
  assert(stack_.size() == 0);
  return *this;
}

// -----------------------------------------------------------------------------
SCCSolver &SCCSolver::Single(SetNode *node)
{
  // Reset the traversal.
  epoch_ += 1;
  index_ = 1ull;

  // Find SCCs starting at this node.
  VisitSingle(node);

  // Stack must be empty after traversal.
  assert(stack_.size() == 0);
  return *this;
}

// -----------------------------------------------------------------------------
void SCCSolver::Solve(std::function<void(const Group &)> &&f)
{
  // Traverse the computed SCCs.
  for (auto &scc : sccs_) {
    f(scc);
  }
  sccs_.clear();
}

// -----------------------------------------------------------------------------
void SCCSolver::VisitFull(GraphNode *node)
{
  node->Epoch = epoch_;
  node->Index = index_;
  node->Link = index_;
  index_ += 1;
  node->InComponent = false;

  if (auto *set = node->AsSet()) {
    for (auto id : set->sets()) {
      auto *v = solver_->Find(id);
      if (v->Epoch != epoch_) {
        VisitFull(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (!v->InComponent) {
        node->Link = std::min(node->Link, v->Link);
      }
    }

    for (auto id : set->derefs()) {
      auto *v = solver_->derefs_[id];
      if (v->Epoch != epoch_) {
        VisitFull(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (!v->InComponent) {
        node->Link = std::min(node->Link, v->Link);
      }
    }
  }

  if (auto *deref = node->AsDeref()) {
    for (auto id : deref->set_outs()) {
      auto *v = solver_->Find(id);
      if (v->Epoch != epoch_) {
        VisitFull(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (!v->InComponent) {
        node->Link = std::min(node->Link, v->Link);
      }
    }
  }

  if (node->Link == node->Index) {
    node->InComponent = true;

    std::vector<GraphNode *> *scc = nullptr;
    while (!stack_.empty() && stack_.top()->Index > node->Link) {
      GraphNode *v = stack_.top();
      stack_.pop();
      v->InComponent = true;
      if (!scc) {
        scc = &sccs_.emplace_back();
      }
      scc->push_back(v);
    }
    if (scc) {
      scc->push_back(node);
    }
  } else {
    stack_.push(node);
  }
}

// -----------------------------------------------------------------------------
void SCCSolver::VisitSingle(SetNode *node)
{
  node->Epoch = epoch_;
  node->Index = index_;
  node->Link = index_;
  index_ += 1;
  node->InComponent = false;

  if (auto *set = node->AsSet()) {
    for (auto id : set->sets()) {
      auto *v = solver_->Find(id);
      if (v->Epoch != epoch_) {
        VisitSingle(v);
        node->Link = std::min(node->Link, v->Link);
      } else if (!v->InComponent) {
        node->Link = std::min(node->Link, v->Link);
      }
    }
  }

  if (node->Link == node->Index) {
    node->InComponent = true;

    std::vector<GraphNode *> *scc = nullptr;
    while (!stack_.empty() && stack_.top()->Index > node->Link) {
      GraphNode *v = stack_.top();
      stack_.pop();
      v->InComponent = true;
      if (!scc) {
        scc = &sccs_.emplace_back();
      }
      scc->push_back(v);
    }
    if (scc) {
      scc->push_back(node);
    }
  } else {
    stack_.push(node);
  }
}
