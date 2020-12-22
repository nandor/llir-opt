// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <vector>
#include "core/block.h"
#include "core/cast.h"
#include "core/insts.h"
#include "core/insts/call.h"
#include "core/insts/control.h"
#include "core/func.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "passes/dead_func_elim.h"
#include "passes/pta.h"



// -----------------------------------------------------------------------------
const char *DeadFuncElimPass::kPassID = "dead-func-elim";

// -----------------------------------------------------------------------------
bool DeadFuncElimPass::Run(Prog &prog)
{
  // Root nodes to start search from.
  std::unordered_set<Func *> live;
  std::vector<Func *> queue;

  // Get the points-to analysis.
  auto *pta = getAnalysis<PointsToAnalysis>();

  // Find all functions which are referenced from data sections.
  for (auto &func : prog) {
    if (func.IsRoot()) {
      queue.push_back(&func);
      live.insert(&func);
      continue;
    }

    if (!func.HasAddressTaken()) {
      continue;
    }

    if (pta && !pta->IsReachable(&func)) {
      continue;
    }

    queue.push_back(&func);
    live.insert(&func);
  }

  // Start a search from these root functions.
  while (!queue.empty()) {
    Func *f = queue.back();
    queue.pop_back();

    for (auto &block : *f) {
      for (auto &inst : block) {
        for (Ref<Value> op : inst.operand_values()) {
          if (Ref<Func> funcOpRef = ::cast_or_null<Func>(op)) {
            Func *funcOp = funcOpRef.Get();
            if (live.insert(funcOp).second) {
              queue.push_back(funcOp);
            }
          }
          if (Ref<Block> blockOpRef = ::cast_or_null<Block>(op)) {
            Func *parent = blockOpRef->getParent();
            if (live.insert(parent).second) {
              queue.push_back(parent);
            }
          }
        }
      }
    }
  }

  bool changed = false;
  for (auto ft = prog.begin(); ft != prog.end(); ) {
    Func *f = &*ft++;
    if (live.count(f) != 0) {
      continue;
    }

    if (f->use_empty()) {
      f->eraseFromParent();
      continue;
    }

    if (f->inst_size() != 1 || !f->begin()->begin()->Is(Inst::Kind::TRAP)) {
      f->clear();
      auto *bb = new Block((".L" + f->getName() + "_dead_trap").str());
      f->AddBlock(bb);
      bb->AddInst(new TrapInst({}));
      changed = true;
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *DeadFuncElimPass::GetPassName() const
{
  return "Dead Function Elimination";
}
