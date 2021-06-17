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
  /*
  // int ptr|int ptr|int
  // int val ptr|int
  // ptr int ptr|int
  // ptr|int int ptr|int
  // ptr|int ptr|int ptr|int
  // val int ptr|int
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
  */
  // TODO:
  Infer(i);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitXorInst(XorInst &i)
{
  /*
  // addr|int val ptr|int
  // int ptr|int ptr|int
  // ptr|int int ptr|int
  // ptr|int ptr ptr|int
  // ptr|int ptr|int ptr
  // ptr|int ptr|int ptr|int
  // val val val
  // ptr int int
  // int int int

  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
  */
  // TODO:
  Infer(i);
}
