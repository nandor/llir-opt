// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/phi_elim.h"

using InstSet = llvm::SmallPtrSet<PhiInst *, 16>;



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
static bool IsDeadPhiCycle(PhiInst *phi, InstSet &phis)
{
  if (!phis.insert(phi).second) {
    return true;
  }

  for (User *user : phi->users()) {
    if (!user->Is(Value::Kind::INST)) {
      return false;
    }
    auto *inst = static_cast<Inst *>(user);
    if (!inst->Is(Inst::Kind::PHI)) {
      return false;
    }
    if (!IsDeadPhiCycle(static_cast<PhiInst *>(inst), phis)) {
      return false;
    }
  }

  return true;
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

      InstSet phiSet;
      if (IsDeadPhiCycle(static_cast<PhiInst *>(inst), phiSet)) {
        for (auto *inst : phiSet) {
          if (inst == &*it) {
            ++it;
          }
          inst->eraseFromParent();
        }
      }
    }
  }
}
