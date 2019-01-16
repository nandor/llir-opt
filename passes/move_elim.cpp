// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/move_elim.h"



// -----------------------------------------------------------------------------
void MoveElimPass::Run(Prog *prog)
{
  for (auto &func : *prog) {
    for (auto &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        Inst &inst = *it++;
        if (!inst.Is(Inst::Kind::MOV)) {
          continue;
        }

        auto &movInst = static_cast<MovInst&>(inst);
        auto *arg = movInst.GetArg();
        if (!arg->Is(Value::Kind::INST)) {
          continue;
        }

        auto *argInst = static_cast<Inst *>(arg);
        if (argInst->GetType(0) != movInst.GetType()) {
          continue;
        }

        // Since in this form we have PHIs, moves which rename
        // virtual registers are not required and can be replaced
        // with the virtual register they copy from.
        movInst.replaceAllUsesWith(argInst);
        movInst.eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *MoveElimPass::GetPassName() const
{
  return "Move Elimination";
}
