// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>
#include <stack>

#include "passes/tags/sat.h"



// -----------------------------------------------------------------------------
SATProblem::SATNSolver::SATNSolver(const ClauseList &list)
  : decisionLevel_(0)
{
  // Set up the literals & clauses.
  unsigned size = 0;
  for (auto &clause : list) {
    auto &c = clauses_.emplace_back();
    for (auto lit : clause) {
      size = std::max(size, (lit >> 1) + 1);
      c.Lits.push_back(lit);
    }
  }
  for (unsigned i = 0; i < size; ++i) {
    lits_.emplace_back(i);
  }
}

// -----------------------------------------------------------------------------
bool SATProblem::SATNSolver::IsSatisfiable()
{
  while (auto lit = PickBranchingVariable()) {
    while (FindConflict()) {
      if (decisionLevel_ == 0) {
        return false;
      }
      Backtrack();
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
bool SATProblem::SATNSolver::IsSatisfiableWith(ID<Lit> id)
{
  if (id > lits_.size()) {
    return true;
  }

  lits_[id].Assign(true);

  while (auto lit = PickBranchingVariable()) {
    while (FindConflict()) {
      if (decisionLevel_ == 0) {
        lits_[id].Unassign();
        return false;
      }
      Backtrack();
    }
  }

  lits_[id].Unassign();
  return true;
}

// -----------------------------------------------------------------------------
bool SATProblem::SATNSolver::FindConflict()
{
  for (auto &clause : clauses_) {
    bool hasTrue = false;
    bool hasUndef = false;
    for (auto &lit : clause.Lits) {
      auto *l = &lits_[lit >> 1];
      switch (l->GetState()) {
        case Literal::State::UNDEF: {
          hasUndef = true;
          continue;
        }
        case Literal::State::TRUE: {
          hasTrue |= (lit & 1) == 0;
          continue;
        }
        case Literal::State::FALSE: {
          hasTrue |= (lit & 1) == 1;
          continue;
        }
      }
      llvm_unreachable("unknown state");
    }
    if (!hasTrue && !hasUndef) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
void SATProblem::SATNSolver::Backtrack()
{
  Literal *lit = nullptr;
  while (*trail_.rbegin()) {
    lit = *trail_.rbegin();
    trail_.pop_back();
    lit->Unassign();
  }

  trail_.pop_back();
  --decisionLevel_;
  lit->Assign(false);
  trail_.push_back(lit);
}

// -----------------------------------------------------------------------------
SATProblem::SATNSolver::Literal *SATProblem::SATNSolver::PickBranchingVariable()
{
  Literal *lit = nullptr;
  for (auto &l : lits_) {
    if (!l.IsAssigned()) {
      lit = &l;
      break;
    }
  }
  if (!lit) {
    return nullptr;
  }
  ++decisionLevel_;
  trail_.push_back(nullptr);
  lit->Assign(true);
  trail_.push_back(lit);
  return lit;
}
