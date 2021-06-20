// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/Statistic.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/move_elim.h"

#define DEBUG_TYPE "move-elim"

STATISTIC(NumMovsForwarded, "Number of movs forwarded");



// -----------------------------------------------------------------------------
const char *MoveElimPass::kPassID = "move-elim";

// -----------------------------------------------------------------------------
static bool CanEliminate(Ref<MovInst> mov, Ref<Inst> arg)
{
  if (mov->GetType() == arg.GetType()) {
    return true;
  }
  if (auto argMov = ::cast_or_null<MovInst>(arg)) {
    return !::cast_or_null<Inst>(argMov->GetArg());
  }
  return false;
}

// -----------------------------------------------------------------------------
bool MoveElimPass::Run(Prog &prog)
{
  bool changed = false;
  for (auto &func : prog) {
    for (auto *block : llvm::ReversePostOrderTraversal<Func*>(&func)) {
      for (auto it = block->begin(); it != block->end(); ) {
        if (auto *mov = ::cast_or_null<MovInst>(&*it++)) {
          if (Ref<Inst> arg = ::cast_or_null<Inst>(mov->GetArg())) {
            if (CanEliminate(mov, arg)) {
              // Since in this form we have PHIs, moves which rename
              // virtual registers are not required and can be replaced
              // with the virtual register they copy from.
              mov->replaceAllUsesWith(arg);
              mov->eraseFromParent();
              changed = true;
              ++NumMovsForwarded;
              continue;
            }
          }
        }
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *MoveElimPass::GetPassName() const
{
  return "Move Elimination";
}
