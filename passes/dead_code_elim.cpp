// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/dominator.h"
#include "core/insts_control.h"
#include "core/func.h"
#include "core/prog.h"
#include "passes/dead_code_elim.h"



// -----------------------------------------------------------------------------
void DeadCodeElimPass::Run(Prog *prog)
{
  // Remove dead instructions in individual functions.
  for (auto &func : *prog) {
    Run(&func);
  }

  // Remove dead functions.
  {
    for (auto ft = prog->begin(); ft != prog->end(); ) {
      Func *func = &*ft++;
      if (func->use_empty() && func->IsHidden()) {
        func->eraseFromParent();
      }
    }
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
      if (opVal->Is(Value::Kind::GLOBAL)) {
        if (static_cast<Global *>(opVal)->Is(Global::Kind::BLOCK)) {
          if (auto *term = static_cast<Block *>(opVal)->GetTerminator()) {
            if (marked.insert(term).second) {
              work.push(term);
            }
          }
        }
      }
      if (opVal->Is(Value::Kind::INST)) {
        auto *opInst = static_cast<Inst *>(opVal);
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
            } while (!useful.count(node->getBlock()));

            block.AddInst(new JumpInst(node->getBlock()), inst);
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

  // Remove dead blocks.
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

    for (auto it = func->begin(); it != func->end(); ) {
      Block *block = &*it++;
      if (blocks.count(block) == 0) {
        for (auto *succ : block->successors()) {
          assert(!"not implemented");
        }
        block->eraseFromParent();
      }
    }
  }
}
