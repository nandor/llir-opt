// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"



// -----------------------------------------------------------------------------
SymbolicValue SymbolicContext::operator[](Inst *inst) const
{
  abort();
}

// -----------------------------------------------------------------------------
bool SymbolicContext::IsStoreFolded(StoreInst *st) const
{
  abort();
}