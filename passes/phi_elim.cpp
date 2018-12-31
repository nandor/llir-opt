// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "passes/phi_elim.h"



// -----------------------------------------------------------------------------
void PhiElimPass::Run(Prog *prog)
{
  for (auto &func : *prog) {
    Run(&func);
  }
}

// -----------------------------------------------------------------------------
const char *PhiElimPass::GetPassName() const
{
  return "Phi Elimination";
}

// -----------------------------------------------------------------------------
void PhiElimPass::Run(Func *func)
{
  for (auto &block : *func) {
    for (auto it = block.begin(); it != block.end(); ) {
      Inst *inst = &*it++;

      if (!inst->Is(Inst::Kind::PHI)) {
        break;
      }

      if (inst->use_empty()) {
        inst->eraseFromParent();
      }
    }
  }
}
