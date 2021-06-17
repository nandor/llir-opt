// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/constraints.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
void ConstraintSolver::VisitExtensionInst(ExtensionInst &i)
{
  // TODO:
  Infer(i);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitTruncInst(TruncInst &i)
{
  // TODO:
  Infer(i);
}
