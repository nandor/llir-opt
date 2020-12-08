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
void SimplifyCfgPass::Run(Func *func)
{
  EliminateConditionalJumps(*func);
  ThreadJumps(*func);
  FoldBranches(*func);
  RemoveSinglePhis(*func);
  MergeIntoPredecessor(*func);
  func->RemoveUnreachable();
}

// -----------------------------------------------------------------------------
void SimplifyCfgPass::EliminateConditionalJumps(Func &func)
{
  for (auto &block : func) {
    if (auto *jc = ::cast_or_null<JumpCondInst>(block.GetTerminator())) {
      if (jc->GetTrueTarget() == jc->GetFalseTarget()) {
        JumpInst *jmp = new JumpInst(jc->GetTrueTarget(), jc->GetAnnots());
        block.AddInst(jmp, jc);
        jc->replaceAllUsesWith(jmp);
        jc->eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
static Block *FindThread(Block *start, Block *prev, Block **phi, Block *block)
{
  *phi = prev;
  if (block == start) {
    return block;
  }

  if (block->size() != 1) {
    return block;
  }

  if (auto *jmp = ::cast_or_null<JumpInst>(block->GetTerminator())) {
    auto *target = jmp->GetTarget();
    if (target == block) {
      return block;
    }
    return FindThread(start, block, phi, jmp->GetTarget());
  }
  return block;
}

// -----------------------------------------------------------------------------
static Block *Thread(Block *block, Block **pred, Block *original)
{
  auto *target = FindThread(block, block, pred, original);
  if (original == target) {
    return nullptr;
  }
  return target;
}

// -----------------------------------------------------------------------------
static void AddEdge(Block *block, Block *pred, Block *target)
{
  for (PhiInst &phi : target->phis()) {
    if (!phi.HasValue(block) && phi.HasValue(pred)) {
      phi.Add(block, phi.GetValue(pred));
    }
  }
}

// -----------------------------------------------------------------------------
void SimplifyCfgPass::ThreadJumps(Func &func)
{
  for (auto &block : func) {
    if (auto *term = block.GetTerminator()) {
      Inst *newInst = nullptr;
      if (auto *jc = ::cast_or_null<JumpCondInst>(term)) {
        Ref<Inst> cond = jc->GetCond();
        Block *bt = jc->GetTrueTarget();
        Block *bf = jc->GetFalseTarget();

        Block *predTrue, *predFalse;
        auto *threadedT = Thread(&block, &predTrue, bt);
        if (threadedT) {
          for (auto &phi : bt->phis()) {
            phi.Remove(&block);
          }
        }

        auto *threadedF = Thread(&block, &predFalse, bf);
        if (threadedF) {
          for (auto &phi : bf->phis()) {
            phi.Remove(&block);
          }
        }

        if (threadedT || threadedF) {
          auto *newT = threadedT ? threadedT : bt;
          auto *newF = threadedF ? threadedF : bf;
          if (newT != newF) {
            if (bt != newT) {
              AddEdge(&block, predTrue, newT);
            }
            if (bf != newF) {
              AddEdge(&block, predFalse, newF);
            }
            newInst = new JumpCondInst(cond, newT, newF, jc->GetAnnots());
          } else {
            for (PhiInst &phi : newT->phis()) {
              if (predTrue != predFalse) {
                auto *select = new SelectInst(
                    phi.GetType(),
                    cond,
                    phi.GetValue(predTrue),
                    phi.GetValue(predFalse),
                    phi.GetAnnots()
                );
                block.AddInst(select, jc);
                phi.Add(&block, select);
              } else {
                phi.Add(&block, phi.GetValue(predTrue));
              }
            }
            newInst = new JumpInst(newT, jc->GetAnnots());
          }
        }
      }

      if (auto *jmp = ::cast_or_null<JumpInst>(term)) {
        Block *pred;
        if (auto *target = Thread(&block, &pred, jmp->GetTarget())) {
          AddEdge(&block, pred, target);
          for (auto &phi : jmp->GetTarget()->phis()) {
            phi.Remove(&block);
          }
          newInst = new JumpInst(target, jmp->GetAnnots());
        }
      }

      if (auto *call = ::cast_or_null<CallInst>(term)) {
        Block *pred;
        if (auto *target = Thread(&block, &pred, call->GetCont())) {
          AddEdge(&block, pred, target);
          for (auto &phi : call->GetCont()->phis()) {
            phi.Remove(&block);
          }
          newInst = new CallInst(
              std::vector<Type>(call->type_begin(), call->type_end()),
              call->GetCallee(),
              std::vector<Ref<Inst>>(call->arg_begin(), call->arg_end()),
              call->GetFlags(),
              target,
              call->GetNumFixedArgs(),
              call->GetCallingConv(),
              call->GetAnnots()
          );
        }
      }

      if (newInst) {
        block.AddInst(newInst, term);
        term->replaceAllUsesWith(newInst);
        term->eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
void SimplifyCfgPass::FoldBranches(Func &func)
{
  for (auto &block : func) {
    if (auto *inst = ::cast_or_null<JumpCondInst>(block.GetTerminator())) {
      bool foldTrue = false;
      bool foldFalse = false;

      if (Ref<MovInst> mov = ::cast_or_null<MovInst>(inst->GetCond())) {
        Value *val = mov->GetArg().Get();
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
            foldFalse = false;
            break;
          }
        }
      }

      Inst *newInst = nullptr;
      if (foldTrue) {
        newInst = new JumpInst(inst->GetTrueTarget(), inst->GetAnnots());
        for (auto &phi : inst->GetFalseTarget()->phis()) {
          phi.Remove(&block);
        }
      }
      if (foldFalse) {
        newInst = new JumpInst(inst->GetFalseTarget(), inst->GetAnnots());
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

    if (auto *inst = ::cast_or_null<SwitchInst>(block.GetTerminator())) {
      if (Ref<MovInst> mov = ::cast_or_null<MovInst>(inst->GetIdx())) {
        if (Ref<ConstantInt> val = ::cast_or_null<ConstantInt>(mov->GetArg())) {
          int64_t idx = val->GetValue().getSExtValue();
          unsigned n = inst->getNumSuccessors();

          Inst *newInst;
          if (idx < 0 || n <= idx) {
            newInst = new TrapInst({});
          } else {
            newInst = new JumpInst(inst->getSuccessor(idx), inst->GetAnnots());
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
}

// -----------------------------------------------------------------------------
void SimplifyCfgPass::RemoveSinglePhis(Func &func)
{
  for (auto &block : func) {
    for (auto it = block.begin(); it != block.end(); ) {
      auto *inst = &*it++;
      if (auto *phi = ::cast_or_null<PhiInst>(inst)) {
        if (phi->GetNumIncoming() == 1) {
          phi->replaceAllUsesWith(phi->GetValue(0u));
          phi->eraseFromParent();
        }
      } else {
        break;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void SimplifyCfgPass::MergeIntoPredecessor(Func &func)
{
  for (auto bt = ++func.begin(); bt != func.end(); ) {
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
    if (block->HasAddressTaken()) {
      continue;
    }

    // Erase the terminator & transfer all instructions.
    pred->GetTerminator()->eraseFromParent();
    for (auto it = block->begin(); it != block->end(); ) {
      auto *inst = &*it++;
      if (auto *phi = ::cast_or_null<PhiInst>(inst)) {
        assert(phi->GetNumIncoming() == 1 && "invalid phi");
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
