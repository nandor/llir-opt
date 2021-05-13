// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/target.h"
#include "passes/eliminate_tags.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
const char *EliminateTagsPass::kPassID = "eliminate-tags";

// -----------------------------------------------------------------------------
bool EliminateTagsPass::Run(Prog &prog)
{
  TypeAnalysis analysis(prog, GetTarget());
  analysis.Solve();
  analysis.dump();

  bool changed = false;
  for (Func &func : prog) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        if (inst.Is(Inst::Kind::MOV)) {
          continue;
        }

        llvm::SmallVector<bool, 4> used(inst.GetNumRets());
        for (Use &use : inst.uses()) {
          used[(*use).Index()] = true;
        }
        for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
          if (used[i]) {
            auto val = analysis.Find(inst.GetSubValue(i));
            if (val.IsOne()) {
              llvm::errs() << "1 " << func.getName() << " " << block.getName() << " " << inst << "\n";
            }
            if (val.IsZero()) {
              llvm::errs() << "0 " << func.getName() << " " << block.getName() << " " << inst << "\n";
            }
          }
        }
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *EliminateTagsPass::GetPassName() const
{
  return "Eliminate Tagged Integers";
}
