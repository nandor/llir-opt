// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <vector>
#include "core/block.h"
#include "core/cast.h"
#include "core/insts.h"
#include "core/insts_call.h"
#include "core/insts_control.h"
#include "core/func.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "passes/dead_func_elim.h"
#include "passes/pta.h"



// -----------------------------------------------------------------------------
const char *DeadFuncElimPass::kPassID = "dead-func-elim";

// -----------------------------------------------------------------------------
void DeadFuncElimPass::Run(Prog *prog)
{
  // Root nodes to start search from.
  std::unordered_set<Func *> live;
  std::vector<Func *> queue;

  // Get the points-to analysis.
  auto *pta = getAnalysis<PointsToAnalysis>();

  // Find all functions which are referenced from data sections.
  for (auto &func : *prog) {
    if (func.GetVisibility() == Visibility::EXTERN) {
      queue.push_back(&func);
      live.insert(&func);
      continue;
    }

    bool hasAddressTaken = false;
    for (auto *user : func.users()) {
      if (!user) {
        hasAddressTaken = true;
        break;
      }
    }

    if (!hasAddressTaken) {
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
        for (auto *op : inst.operand_values()) {
          if (auto *funcOp = ::dyn_cast_or_null<Func>(op)) {
            if (live.insert(funcOp).second) {
              queue.push_back(funcOp);
            }
          }
        }
      }
    }
  }

  for (auto ft = prog->begin(); ft != prog->end(); ) {
    Func *f = &*ft++;
    if (live.count(f) == 0) {
      f->clear();
      auto *bb = new Block((".L" + f->getName() + "_entry").str());
      f->AddBlock(bb);
      bb->AddInst(new TrapInst({}));
    }
  }
}

// -----------------------------------------------------------------------------
const char *DeadFuncElimPass::GetPassName() const
{
  return "Dead Function Elimination";
}
