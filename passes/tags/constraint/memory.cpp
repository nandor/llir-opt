// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/constraints.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
void ConstraintSolver::VisitMemoryExchangeInst(MemoryExchangeInst &i)
{
  AnyPointer(i.GetAddr());
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i)
{
  AnyPointer(i.GetAddr());
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitMemoryStoreInst(MemoryStoreInst &store)
{
  AnyPointer(store.GetAddr());
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitMemoryLoadInst(MemoryLoadInst &load)
{
  AnyPointer(load.GetAddr());
  Infer(load);
}
