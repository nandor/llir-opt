// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/insts.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"
#include "passes/dead_code_elim.h"


// -----------------------------------------------------------------------------
const char *DeadCodeElimPass::kPassID = "dead-code-elim";

// -----------------------------------------------------------------------------
bool DeadCodeElimPass::Run(Prog &prog)
{
  bool changed = false;
  for (auto &func : prog) {
    changed = Run(func) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *DeadCodeElimPass::GetPassName() const
{
  return "Dead Code Elimination";
}

// -----------------------------------------------------------------------------
bool DeadCodeElimPass::Run(Func &func)
{
  PostDominatorTree PDT(func);
  PostDominanceFrontier PDF;
  PDF.analyze(PDT);

  std::set<Inst *> marked;
  std::queue<Inst *> work;
  std::set<Block *> useful;

  for (auto &block : func) {
    for (auto &inst : block) {
      // Mark all instructions which are critical.
      if (inst.HasSideEffects()) {
        marked.insert(&inst);
        work.push(&inst);
      }
    }
  }

  // Mark useful instructions through a work-list algorithm.
  while (!work.empty()) {
    Inst *inst = work.front();
    work.pop();

    // Add the operands to the worklist if they were not marked before.
    for (Ref<Value> opVal : inst->operand_values()) {
      if (!opVal) {
        continue;
      }

      if (Ref<Block> opBlockRef = ::cast_or_null<Block>(opVal)) {
        Block *opBlock = opBlockRef.Get();
        if (auto *term = opBlock->GetTerminator()) {
          if (marked.insert(term).second) {
            work.push(term);
          }
        }
      }

      if (Ref<Inst> opInstRef = ::cast_or_null<Inst>(opVal)) {
        Inst *opInst = opInstRef.Get();
        if (marked.insert(opInst).second) {
          work.push(opInst);
        }
      }
    }

    // Add the terminators of the reverse dominance frontier.
    Block *parent = inst->getParent();
    if (useful.insert(parent).second) {
      if (auto *node = PDT.getNode(parent)) {
        for (auto &block : PDF.calculate(PDT, node)) {
          if (auto *term = block->GetTerminator()) {
            if (marked.insert(term).second) {
              work.push(term);
            }
          }
        }
      }
    }
  }

  // Helper to find the post-dominator where a jump must reach.
  auto findTarget = [&, this] (Inst *inst)
  {
    auto *node = PDT.getNode(inst->getParent());
    do {
      node = node->getIDom();
    } while (node->getBlock() && !useful.count(node->getBlock()));
    return node->getBlock();
  };

  // Remove unmarked instructions.
  bool changed = false;
  for (auto &block : func) {
    for (auto it = block.rbegin(); it != block.rend(); ) {
      Inst *inst = &*it++;
      if (marked.count(inst) != 0) {
        continue;
      }

      switch (inst->GetKind()) {
        case Inst::Kind::JUMP: {
          if (auto *target = findTarget(inst)) {
            if (target != static_cast<JumpInst *>(inst)->GetTarget()) {
              block.AddInst(new JumpInst(target, inst->GetAnnots()));
              inst->eraseFromParent();
              changed = true;
            }
          } else {
            block.AddInst(new TrapInst(inst->GetAnnots()));
            inst->eraseFromParent();
            changed = true;
          }
          break;
        }
        case Inst::Kind::JUMP_COND: {
          if (auto *target = findTarget(inst)) {
            block.AddInst(new JumpInst(target, inst->GetAnnots()));
          } else {
            block.AddInst(new TrapInst(inst->GetAnnots()));
          }
          inst->eraseFromParent();
          changed = true;
          break;
        }
        default: {
          inst->eraseFromParent();
          changed = true;
          break;
        }
      }
    }
  }
  return changed;
}
