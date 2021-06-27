// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <list>
#include <set>

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
    SATNSolver(const ClauseList &list);

    bool IsSatisfiable();

    bool IsSatisfiableWith(ID<Lit> id);

  private:
    struct Clause {
      /// Literals in the clause.
      llvm::SmallVector<unsigned, 4> Lits;
    };

    /// Description of a literal.
    class Literal {
    public:
      /// Value of the literal.
      enum class State {
        FALSE,
        TRUE,
        UNDEF,
      };

      /// Set up the literal.
      Literal(unsigned id)
        : id_(id)
        , state_(State::UNDEF)
      {
      }

      /// Return the ID of the literal.
      unsigned GetId() const { return id_; }
      /// Return the state of the value.
      State GetState() const { return state_; }
      /// Check whether the literal was assigned.
      bool IsAssigned() const { return state_ != State::UNDEF; }
      /// Check whether the literal was assigned true.
      bool IsTrue() const { return state_ != State::TRUE; }
      /// Check whether the literal was assigned false.
      bool IsFalse() const { return state_ != State::FALSE; }
      /// Assign a value to the lit.
      void Assign(bool v)
      {
        state_ = v ? State::TRUE : State::FALSE;
      }
      /// Set the state to undef.
      void Unassign() { state_ = State::UNDEF; }

    private:
      /// ID of the literal.
      unsigned id_;
      /// State of the literal.
      State state_;
    };

  private:
    Literal *PickBranchingVariable();

    bool FindConflict();

    void Backtrack();

  private:
    /// Clauses of the system.
    std::vector<Clause> clauses_;
    /// Watch list for each variable.
    std::vector<Literal> lits_;
    /// Trail of assignments.
    std::vector<Literal *> trail_;
    /// Current decision level.
    unsigned decisionLevel_;
    /// Number of satisfied literals.
    unsigned satisfied_;
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
