// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/constraints.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
void ConstraintSolver::VisitPhiInst(PhiInst &phi)
{
  for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
    Subset(phi.GetValue(i), phi.GetSubValue(0));
  }
  Infer(phi);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitSelectInst(SelectInst &i)
{
  Subset(i.GetTrue(), i.GetSubValue(0));
  Subset(i.GetFalse(), i.GetSubValue(0));
  Infer(i);
}
