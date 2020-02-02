// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/simplify_cfg.h"



// -----------------------------------------------------------------------------
const char *SimplifyCfgPass::kPassID = "simplify-cfg";

// -----------------------------------------------------------------------------
void SimplifyCfgPass::Run(Prog *prog)
{
  for (Func &func : *prog) {
    Run(&func);
  }
}

// -----------------------------------------------------------------------------
const char *SimplifyCfgPass::GetPassName() const
{
  return "Control Flow Simplification";
}

// -----------------------------------------------------------------------------
void SimplifyCfgPass::Run(Func *func)
{
  // Fold branches with known arguments.
  for (auto &block : *func) {
    if (auto *jccInst = ::dyn_cast_or_null<JumpCondInst>(block.GetTerminator())) {
      if (auto *movInst = ::dyn_cast_or_null<MovInst>(jccInst->GetCond())) {
        bool foldTrue = false;
        bool foldFalse = false;

        auto *val = movInst->GetArg();
        switch (val->GetKind()) {
          case Value::Kind::INST: {
            continue;
          }
          case Value::Kind::CONST: {
            switch (static_cast<Constant *>(val)->GetKind()) {
              case Constant::Kind::REG: {
                continue;
              }
              case Constant::Kind::INT: {
                const auto &iv = static_cast<ConstantInt *>(val)->GetValue();
                foldFalse = iv.isNullValue();
                foldTrue = !foldFalse;
                break;
              }
              case Constant::Kind::FLOAT: {
                const auto &fv = static_cast<ConstantFloat *>(val)->GetValue();
                foldFalse = fv.isZero();
                foldTrue = !foldFalse;
                break;
              }
            }
            break;
          }
          case Value::Kind::GLOBAL:
          case Value::Kind::EXPR: {
            foldTrue = true;
            break;
          }
        }
        Inst *newInst = nullptr;
        if (foldTrue) {
          newInst = new JumpInst(jccInst->GetTrueTarget(), jccInst->GetAnnot());
          for (auto &phi : jccInst->GetFalseTarget()->phis()) {
            phi.Remove(&block);
          }
        }
        if (foldFalse) {
          newInst = new JumpInst(jccInst->GetFalseTarget(), jccInst->GetAnnot());
          for (auto &phi : jccInst->GetTrueTarget()->phis()) {
            phi.Remove(&block);
          }
        }
        if (newInst) {
          block.AddInst(newInst, jccInst);
          jccInst->replaceAllUsesWith(newInst);
          jccInst->eraseFromParent();
        }
      }
    }
  }

  // Remove PHIs with a single incoming node.
  for (auto &block : *func) {
    for (auto it = block.begin(); it != block.end(); ) {
      auto *inst = &*it++;
      if (auto *phi = ::dyn_cast_or_null<PhiInst>(inst)) {
        if (phi->GetNumIncoming() == 1) {
          phi->replaceAllUsesWith(phi->GetValue(0u));
          phi->eraseFromParent();
        }
      } else {
        break;
      }
    }
  }

  // Remove trivially dead blocks.
  {
    llvm::SmallPtrSet<Block *, 10> blocks;

    std::function<void(Block *)> dfs = [&blocks, &dfs] (Block *block) {
      if (!blocks.insert(block).second) {
        return;
      }
      for (auto *succ : block->successors()) {
        dfs(succ);
      }
    };

    dfs(&func->getEntryBlock());

    for (auto &block : *func) {
      if (blocks.count(&block) == 0) {
        for (auto *succ : block.successors()) {
          for (auto &phi : succ->phis()) {
            phi.Remove(&block);
          }
        }
      }
    }

    for (auto it = func->begin(); it != func->end(); ) {
      Block *block = &*it++;
      if (blocks.count(block) == 0) {
        block->replaceAllUsesWith(new ConstantInt(0));
        block->eraseFromParent();
      }
    }
  }

  // Merge basic blocks into predecessors if they have one successor.
  for (auto bt = ++func->begin(); bt != func->end(); ) {
    // Do not merge multiple blocks with preds.
    auto *block = &*bt++;
    if (block->pred_size() != 1) {
      continue;
    }
    // Do not merge if predecessor diverges.
    Block *pred = *block->pred_begin();
    if (pred->succ_size() != 1 || *pred->succ_begin() != block) {
      continue;
    }
    // Do not merge drops which have the address taken.
    bool hasAddressTaken = false;
    for (auto *user : block->users()) {
      hasAddressTaken = ::dyn_cast_or_null<MovInst>(user);
      if (hasAddressTaken) {
        break;
      }
    }
    if (hasAddressTaken) {
      continue;
    }

    // Erase the terminator & transfer all instructions.
    pred->GetTerminator()->eraseFromParent();
    for (auto it = block->begin(); it != block->end(); ) {
      auto *inst = &*it++;
      if (auto *phi = ::dyn_cast_or_null<PhiInst>(inst)) {
        assert(phi->GetBlock(0u) == pred && "invalid predecessor");
        phi->replaceAllUsesWith(phi->GetValue(0u));
        phi->eraseFromParent();
      } else {
        inst->removeFromParent();
        pred->AddInst(inst);
      }
    }
    block->replaceAllUsesWith(pred);
    block->eraseFromParent();
  }
}
