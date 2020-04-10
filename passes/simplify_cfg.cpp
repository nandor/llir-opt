// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
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
static Block *FindThread(Block *start, Block *prev, Block **phi, Block *block) {
  *phi = prev;
  if (block == start) {
    return block;
  }

  if (block->size() != 1) {
    return block;
  }

  if (auto *jmp = ::dyn_cast_or_null<JumpInst>(block->GetTerminator())) {
    return FindThread(start, block, phi, jmp->GetTarget());
  }
  return block;
}

// -----------------------------------------------------------------------------
static Block *Thread(Block *block, Block *original) {
  Block *pred = nullptr;
  auto *target = FindThread(block, block, &pred, original);
  if (original == target) {
    return nullptr;
  }
  for (PhiInst &phi : target->phis()) {
    if (!phi.HasValue(block)) {
      phi.Add(block, phi.GetValue(pred));
    }
  }
  return target;
}

// -----------------------------------------------------------------------------
void SimplifyCfgPass::Run(Func *func)
{
  // Thread jumps.
  for (auto &block : *func) {
    if (auto *term = block.GetTerminator()) {
      Inst *newInst = nullptr;
      if (auto *jc = ::dyn_cast_or_null<JumpCondInst>(term)) {
        auto *threadedT = Thread(&block, jc->GetTrueTarget());
        auto *threadedF = Thread(&block, jc->GetFalseTarget());
        if (threadedT || threadedF) {
          auto *newT = threadedT ? threadedT : jc->GetTrueTarget();
          auto *newF = threadedF ? threadedF : jc->GetFalseTarget();
          if (newT != newF) {
            newInst = new JumpCondInst(jc->GetCond(), newT, newF, jc->GetAnnot());
          }
        }
      }

      if (auto *jmp = ::dyn_cast_or_null<JumpInst>(term)) {
        if (auto *target = Thread(&block, jmp->GetTarget())) {
          newInst = new JumpInst(target, jmp->GetAnnot());
        }
      }

      if (newInst) {
        block.AddInst(newInst, term);
        term->replaceAllUsesWith(newInst);
        term->eraseFromParent();
      }
    }
  }

  // Fold branches with known arguments.
  for (auto &block : *func) {
    if (auto *inst = ::dyn_cast_or_null<JumpCondInst>(block.GetTerminator())) {
      if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst->GetCond())) {
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
          newInst = new JumpInst(inst->GetTrueTarget(), inst->GetAnnot());
          for (auto &phi : inst->GetFalseTarget()->phis()) {
            phi.Remove(&block);
          }
        }
        if (foldFalse) {
          newInst = new JumpInst(inst->GetFalseTarget(), inst->GetAnnot());
          for (auto &phi : inst->GetTrueTarget()->phis()) {
            phi.Remove(&block);
          }
        }
        if (newInst) {
          block.AddInst(newInst, inst);
          inst->replaceAllUsesWith(newInst);
          inst->eraseFromParent();
        }
      }
    }

    if (auto *inst = ::dyn_cast_or_null<SwitchInst>(block.GetTerminator())) {
      if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst->GetIdx())) {
        if (auto *intConst = ::dyn_cast_or_null<ConstantInt>(movInst->GetArg())) {
          int64_t idx = intConst->GetValue().getSExtValue();
          unsigned n = inst->getNumSuccessors();

          Inst *newInst;
          if (idx < 0 || n <= idx) {
            newInst = new TrapInst({});
          } else {
            newInst = new JumpInst(inst->getSuccessor(idx), inst->GetAnnot());
          }

          for (unsigned i = 0; i < n; ++i) {
            if (i != idx) {
              Block *block = inst->getSuccessor(i);
              for (auto &phi : block->phis()) {
                phi.Remove(block);
              }
            }
          }

          block.AddInst(newInst, inst);
          inst->replaceAllUsesWith(newInst);
          inst->eraseFromParent();
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
          auto *value = phi->GetValue(0u);
          if (auto *incoming = ::dyn_cast_or_null<Inst>(value)) {
            for (auto annot : phi->annots()) {
              incoming->SetAnnot(annot);
            }
          }
          phi->replaceAllUsesWith(value);
          phi->eraseFromParent();
        }
      } else {
        break;
      }
    }
  }

  // Remove trivially dead blocks.
  RemoveUnreachable(func);

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
        auto *value = phi->GetValue(0u);
        if (auto *inst = ::dyn_cast_or_null<Inst>(value)) {
          inst->SetAnnot(inst->GetAnnot().Union(phi->GetAnnot()));
        }
        phi->replaceAllUsesWith(value);
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
