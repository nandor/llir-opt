// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/constraints.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
void ConstraintSolver::VisitSubInst(SubInst &i)
{
  // TODO:
  Infer(i);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitAddInst(AddInst &i)
{
  // TODO:
  Infer(i);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitOrInst(OrInst &i)
{
  // TODO:
  Infer(i);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitAndInst(AndInst &i)
{
  // TODO:
  Infer(i);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitXorInst(XorInst &i)
{
  // TODO:
  Infer(i);
}
