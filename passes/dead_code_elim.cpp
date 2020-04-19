// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/insts_control.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"
#include "passes/dead_code_elim.h"


// -----------------------------------------------------------------------------
const char *DeadCodeElimPass::kPassID = "dead-code-elim";

// -----------------------------------------------------------------------------
void DeadCodeElimPass::Run(Prog *prog)
{
  // Remove dead instructions in individual functions.
  for (auto &func : *prog) {
    Run(&func);
  }
}

// -----------------------------------------------------------------------------
const char *DeadCodeElimPass::GetPassName() const
{
  return "Dead Code Elimination";
}

// -----------------------------------------------------------------------------
void DeadCodeElimPass::Run(Func *func)
{
  PostDominatorTree PDT(*func);
  PostDominanceFrontier PDF;
  PDF.analyze(PDT);

  std::set<Inst *> marked;
  std::queue<Inst *> work;

  for (auto &block : *func) {
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
    for (Value *opVal : inst->operand_values()) {
      if (!opVal) {
        continue;
      }

      if (auto *opBlock = ::dyn_cast_or_null<Block>(opVal)) {
        if (auto *term = opBlock->GetTerminator()) {
          if (marked.insert(term).second) {
            work.push(term);
          }
        }
      }

      if (auto *opInst = ::dyn_cast_or_null<Inst>(opVal)) {
        if (marked.insert(opInst).second) {
          work.push(opInst);
        }
      }
    }

    // Add the terminators of the reverse dominance frontier.
    if (auto *node = PDT.getNode(inst->getParent())) {
      for (auto &block : PDF.calculate(PDT, node)) {
        if (auto *term = block->GetTerminator()) {
          if (marked.insert(term).second) {
            work.push(term);
          }
        }
      }
    }
  }

  // Find useful blocks: those which contain marked instructions.
  {
    std::set<Block *> useful;
    for (auto *inst : marked) {
      useful.insert(inst->getParent());
    }

    // Remove unmarked instructions.
    for (auto &block : *func) {
      for (auto it = block.rbegin(); it != block.rend(); ) {
        Inst *inst = &*it++;
        if (marked.count(inst) != 0) {
          continue;
        }

        switch (inst->GetKind()) {
          case Inst::Kind::JMP:
          case Inst::Kind::JCC: {
            auto *jmpInst = static_cast<JumpInst *>(inst);
            auto *node = PDT.getNode(jmpInst->getParent());
            do {
              node = node->getIDom();
            } while (node->getBlock() && !useful.count(node->getBlock()));

            if (!node->getBlock()) {
              block.AddInst(new TrapInst(inst->GetAnnot()));
            } else {
              block.AddInst(new JumpInst(node->getBlock(), inst->GetAnnot()));
            }
            inst->eraseFromParent();
            break;
          }
          default: {
            inst->eraseFromParent();
            break;
          }
        }
      }
    }
  }
}
