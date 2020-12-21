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
#include "passes/peephole.h"



// -----------------------------------------------------------------------------
const char *PeepholePass::kPassID = "peephole";

// -----------------------------------------------------------------------------
const char *PeepholePass::GetPassName() const
{
  return "Peephole Optimisation";
}

// -----------------------------------------------------------------------------
bool PeepholePass::Run(Prog &prog)
{
  bool changed = false;
  for (auto &func : prog) {
    for (auto &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        changed = Dispatch(*it++) || changed;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool PeepholePass::VisitAddInst(AddInst &inst)
{
  if (auto mov = ::cast_or_null<MovInst>(inst.GetRHS())) {
    if (auto ci = ::cast_or_null<ConstantInt>(mov->GetArg())) {
      if (ci->GetValue().isNullValue()) {
        inst.replaceAllUsesWith(inst.GetLHS());
        inst.eraseFromParent();
        return true;
      }
    }
  }
  if (auto mov = ::cast_or_null<MovInst>(inst.GetLHS())) {
    if (auto ci = ::cast_or_null<ConstantInt>(mov->GetArg())) {
      if (ci->GetValue().isNullValue()) {
        inst.replaceAllUsesWith(inst.GetRHS());
        inst.eraseFromParent();
        return true;
      }
    }
  }
  return false;
}
