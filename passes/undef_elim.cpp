// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/undef_elim.h"



// -----------------------------------------------------------------------------
const char *UndefElimPass::kPassID = "undef-elim";

// -----------------------------------------------------------------------------
const char *UndefElimPass::GetPassName() const
{
  return "Undefined Elimination";
}

// -----------------------------------------------------------------------------
void UndefElimPass::Run(Prog *prog)
{
  for (auto &func : *prog) {
    for (auto &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        Inst *inst = &*it++;
        switch (inst->GetKind()) {
          case Inst::Kind::JCC: {
            SimplifyJumpCond(static_cast<JumpCondInst *>(inst));
            continue;
          }
          case Inst::Kind::SWITCH: {
            SimplifySwitch(static_cast<SwitchInst *>(inst));
            continue;
          }
          default: {
            continue;
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void UndefElimPass::SimplifyJumpCond(JumpCondInst *i)
{
  if (::dyn_cast_or_null<UndefInst>(i->GetCond())) {
    Block *block = i->getParent();
    Inst *newInst = new JumpInst(i->GetFalseTarget(), i->GetAnnot());
    for (auto &phi : i->GetTrueTarget()->phis()) {
      phi.Remove(block);
    }
    block->AddInst(newInst, i);
    i->replaceAllUsesWith(newInst);
    i->eraseFromParent();
  }
}

// -----------------------------------------------------------------------------
void UndefElimPass::SimplifySwitch(SwitchInst *i)
{
  if (::dyn_cast_or_null<UndefInst>(i->GetIdx())) {
    Block *block = i->getParent();

    Block *choice = nullptr;
    if (i->getNumSuccessors() == 0) {
      TrapInst *inst = new TrapInst(i->GetAnnot());
      block->AddInst(inst, i);
    } else {
      choice = i->getSuccessor(0);
      JumpInst *inst = new JumpInst(choice, i->GetAnnot());
      block->AddInst(inst, i);
    }

    for (unsigned idx = 0, n = i->getNumSuccessors(); idx < n; ++idx) {
      Block *succ = i->getSuccessor(idx);
      if (succ == choice) {
        continue;
      }
      for (auto &phi : succ->phis()) {
        phi.Remove(block);
      }
    }

    i->eraseFromParent();
  }
}

// -----------------------------------------------------------------------------
void UndefElimPass::SimplifySelect(SelectInst *i)
{
  if (::dyn_cast_or_null<UndefInst>(i->GetCond())) {
    i->replaceAllUsesWith(i->GetFalse());
    i->eraseFromParent();
  }
}

// -----------------------------------------------------------------------------
void UndefElimPass::SimplifyStore(StoreInst *i)
{
  if (::dyn_cast_or_null<UndefInst>(i->GetAddr())) {
    i->eraseFromParent();
  }
}

