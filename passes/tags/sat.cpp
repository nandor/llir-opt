// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <stack>
#include <queue>

#include "passes/tags/sat.h"



// -----------------------------------------------------------------------------
void SATProblem::Add(llvm::ArrayRef<ID<Lit>> pos, llvm::ArrayRef<ID<Lit>> neg)
{
  auto &clause = clauses_.emplace_back();
  for (auto lit : pos) {
    clause.push_back((static_cast<unsigned>(lit) << 1) | 0);
  }
  for (auto lit : neg) {
    clause.push_back((static_cast<unsigned>(lit) << 1) | 1);
  }
  if (clause.size() > 2) {
    is2SAT_ = false;
  }
}

// -----------------------------------------------------------------------------
bool SATProblem::IsSatisfiable()
{
  if (is2SAT_) {
    if (!solver2SAT_) {
      solver2SAT_.reset(new SAT2Solver(clauses_));
    }
    return solver2SAT_->IsSatisfiable();
  } else {
    if (!solverNSAT_) {
      solverNSAT_.reset(new SATNSolver(clauses_));
    }
    return solverNSAT_->IsSatisfiable();
  }
}

// -----------------------------------------------------------------------------
bool SATProblem::IsSatisfiableWith(ID<Lit> id)
{
  if (is2SAT_) {
    if (!solver2SAT_) {
      solver2SAT_.reset(new SAT2Solver(clauses_));
    }
    return solver2SAT_->IsSatisfiableWith(id);
  } else {
    if (!solverNSAT_) {
      solverNSAT_.reset(new SATNSolver(clauses_));
    }
    return solverNSAT_->IsSatisfiableWith(id);
  }
}

// -----------------------------------------------------------------------------
void SATProblem::dump(llvm::raw_ostream &os)
{
  for (auto &clause : clauses_) {
    for (unsigned i = 0, n = clause.size(); i < n; ++i) {
      auto lit = clause[i];
      if (lit & 1) {
        os << "~";
      }
      os << (lit >> 1);
      if (i + 1 != n) {
        os << " \\/ ";
      }
    }
    os << "\n";
  }
}
