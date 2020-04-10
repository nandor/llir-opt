// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cfg.h"
#include "core/insts.h"



// -----------------------------------------------------------------------------
void RemoveUnreachable(Func *func)
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
