// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/adt/bitset.h"


/**
 * n-SAT solver based on DPLL.
 */
class SATProblem final {
public:
  struct Lit;

  void Add(const BitSet<Lit> &pos, const BitSet<Lit> &neg);

  /**
   * Return true if the system is satisfiable.
   */
  bool IsSatisfiable();

  /**
   * Returns true if the system is satisfiable assuming an additional true lit.
   */
  bool IsSatisfiableWith(ID<Lit> id);

private:

};
