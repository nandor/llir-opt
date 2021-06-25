// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/raw_ostream.h>

#include "core/adt/bitset.h"



/**
 * n-SAT solver based on DPLL.
 */
class SATProblem final {
public:
  struct Lit;

  SATProblem() : is2SAT_(true) {}

  void Add(llvm::ArrayRef<ID<Lit>> pos, llvm::ArrayRef<ID<Lit>> neg);

  /**
   * Return true if the system is satisfiable.
   */
  bool IsSatisfiable();

  /**
   * Returns true if the system is satisfiable assuming an additional true lit.
   */
  bool IsSatisfiableWith(ID<Lit> id);

  /**
   * Print the constraint system.
   */
  void dump(llvm::raw_ostream &os = llvm::errs());

private:
  /// Simple type to represent a short list of literals.
  using Clause = llvm::SmallVector<unsigned, 4>;
  /// Collection of clauses.
  using ClauseList = std::vector<llvm::SmallVector<unsigned, 4>>;

  /// Specialised 2-SAT solver.
  class SAT2Solver {
  public:
    SAT2Solver(const ClauseList &list);

    bool IsSatisfiable();

    bool IsSatisfiableWith(ID<Lit> id);

  private:
    /// Flag to indicate whether the default graph is unsatisfiable.
    bool unsat_;
    /// Connections of the collapsed graph.
    std::vector<std::set<unsigned>> sccGraph_;
    /// Mapping from constraints to their SCC nodes.
    std::unordered_map<unsigned, unsigned> sccOfNode_;
  };

  /// Specialised n-SAT solver.
  class SATNSolver {
  public:
    SATNSolver(const ClauseList &list) {}

    bool IsSatisfiable() { return true; }

    bool IsSatisfiableWith(ID<Lit> id) { return true; }
  };

private:
  /// Set of constraints.
  ClauseList clauses_;
  /// Flag to indicate whether the problem is 2-SAT.
  bool is2SAT_;
  /// SAT-2 Solver instance.
  std::unique_ptr<SAT2Solver> solver2SAT_;
  /// sAT-N Solver instance.
  std::unique_ptr<SATNSolver> solverNSAT_;
};
