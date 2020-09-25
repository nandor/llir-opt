// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/move_elim.h"



// -----------------------------------------------------------------------------
const char *MoveElimPass::kPassID = "move-elim";

// -----------------------------------------------------------------------------
void MoveElimPass::Run(Prog *prog)
{
  for (auto &func : *prog) {
    for (auto *block : llvm::ReversePostOrderTraversal<Func*>(&func)) {
      for (auto it = block->begin(); it != block->end(); ) {
        if (auto *mov = ::dyn_cast_or_null<MovInst>(&*it++)) {
          if (auto *arg = ::dyn_cast_or_null<Inst>(mov->GetArg())) {
            if (arg->GetType(0) != mov->GetType()) {
              continue;
            }

            // Ensure the new value maintains annotations.
            if (mov->HasAnnot<CamlAddr>() != arg->HasAnnot<CamlAddr>()) {
              continue;
            }
            if (mov->HasAnnot<CamlValue>() != arg->HasAnnot<CamlValue>()) {
              continue;
            }

            // Since in this form we have PHIs, moves which rename
            // virtual registers are not required and can be replaced
            // with the virtual register they copy from.
            mov->replaceAllUsesWith(arg);
            mov->eraseFromParent();
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *MoveElimPass::GetPassName() const
{
  return "Move Elimination";
}
