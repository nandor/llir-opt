// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>

#include <llvm/ADT/Statistic.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/phi_taut.h"

#define DEBUG_TYPE "phi-taut"

STATISTIC(NumTautPHIs, "Number of tautological PHIs");


// -----------------------------------------------------------------------------
const char *PhiTautPass::kPassID = DEBUG_TYPE;

// -----------------------------------------------------------------------------
bool PhiTautPass::Run(Prog &prog)
{
  bool changed = false;
  for (auto &func : prog) {
    for (auto &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        auto *phi = ::cast_or_null<PhiInst>(&*it++);
        if (!phi) {
          continue;
        }

        std::unordered_set<Ref<Inst>> refs;
        for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
          refs.insert(phi->GetValue(i));
        }
        refs.erase(phi->GetSubValue(0));
        if (refs.size() != 1) {
          continue;
        }
        phi->replaceAllUsesWith(*refs.begin());
        phi->eraseFromParent();
        ++NumTautPHIs;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *PhiTautPass::GetPassName() const
{
  return "Tautological PHI elimination";
}
