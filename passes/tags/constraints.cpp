// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/prog.h"
#include "passes/tags/constraints.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
ConstraintSolver::ConstraintSolver(
    RegisterAnalysis &analysis,
    const Target *target,
    Prog &prog)
  : analysis_(analysis)
  , target_(target)
  , prog_(prog)
{
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Solve()
{
  for (Func &func : prog_) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        Dispatch(inst);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitSubInst(SubInst &i)
{
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitAddInst(AddInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
}
