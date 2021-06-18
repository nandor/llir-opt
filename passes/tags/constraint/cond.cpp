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
  // Independently infer constraints for the PHI node.
  Infer(phi);

  // Add subset constraints if not a cast.
  auto phiTy = analysis_.Find(phi);
  bool cast = false;
  for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
    auto inTy = analysis_.Find(phi.GetValue(i));
    if (!(inTy <= phiTy)) {
      cast = true;
      break;
    }
  }
  if (!cast) {
    for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
      auto ref = phi.GetValue(i);
      auto inTy = analysis_.Find(ref);
      if (inTy <= phiTy) {
        Subset(ref, phi);
      } else {
        assert(!"invalid PHI constraint");
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitSelectInst(SelectInst &i)
{
  Subset(i.GetTrue(), i.GetSubValue(0));
  Subset(i.GetFalse(), i.GetSubValue(0));
  Infer(i);
}
