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
        Inst &inst = *it++;
        if (!inst.Is(Inst::Kind::MOV)) {
          continue;
        }

        auto &movInst = static_cast<MovInst &>(inst);
        auto *arg = movInst.GetArg();
        switch (arg->GetKind()) {
          case Value::Kind::INST: {
            auto *argInst = static_cast<Inst *>(arg);
            if (argInst->GetType(0) != movInst.GetType()) {
              continue;
            }

            // Ensure the new value maintains annotations.
            if (movInst.HasAnnot(CAML_ADDR) != argInst->HasAnnot(CAML_ADDR)) {
              continue;
            }
            if (movInst.HasAnnot(CAML_VALUE) != argInst->HasAnnot(CAML_VALUE)) {
              continue;
            }

            // Since in this form we have PHIs, moves which rename
            // virtual registers are not required and can be replaced
            // with the virtual register they copy from.
            movInst.replaceAllUsesWith(argInst);
            movInst.eraseFromParent();
            break;
          }
          case Value::Kind::GLOBAL:
          case Value::Kind::EXPR: {
            Propagate(&movInst, arg);
            break;
          }
          case Value::Kind::CONST: {
            switch (static_cast<Constant *>(arg)->GetKind()) {
              case Constant::Kind::REG:
                break;
              case Constant::Kind::INT:
              case Constant::Kind::FLOAT:
                Propagate(&movInst, arg);
                break;
            }
            break;
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void MoveElimPass::Propagate(MovInst *inst, Value *value)
{
  for (auto it = inst->use_begin(); it != inst->use_end(); ) {
    Use &use = *it++;
    if (auto *phi = ::dyn_cast_or_null<PhiInst>(use.getUser())) {
      use = value;
    }
  }
}

// -----------------------------------------------------------------------------
const char *MoveElimPass::GetPassName() const
{
  return "Move Elimination";
}
