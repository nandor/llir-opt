// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "passes/dead_code_elim.h"



// -----------------------------------------------------------------------------
void DeadCodeElimPass::Run(Prog *prog)
{
  for (auto &func : *prog) {
    for (auto bt = func.begin(); bt != func.end(); ) {
      Block &block = *bt++;
      if (block.use_empty() && block.getPrevNode() != nullptr) {
        block.eraseFromParent();
        continue;
      }

      for (auto it = block.rbegin(); it != block.rend(); ) {
        Inst &inst = *it++;
        if (inst.use_empty() && !inst.HasSideEffects()) {
          inst.eraseFromParent();
          continue;
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *DeadCodeElimPass::GetPassName() const
{
  return "Dead Code Elimination";
}
