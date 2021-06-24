// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/tags/sat.h"



// -----------------------------------------------------------------------------
void SATProblem::Add(const BitSet<Lit> &pos, const BitSet<Lit> &neg)
{
  //llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SATProblem::IsSatisfiable()
{
  return true;
  //llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SATProblem::IsSatisfiableWith(ID<Lit> id)
{
  return true;
  //llvm_unreachable("not implemented");
}
