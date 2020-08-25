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
#include "passes/pre_eval.h"
#include "passes/pre_eval/tainted_atoms.h"
#include "passes/pre_eval/symbolic_eval.h"



// -----------------------------------------------------------------------------
const char *PreEvalPass::kPassID = "pre-eval";



// -----------------------------------------------------------------------------
static void SimplifyFunction(TaintedAtoms &taints, Func &func)
{
  llvm::ReversePostOrderTraversal<Func *> rpot(&func);

  SymbolicContext context;
  for (Block *block : rpot) {
    if (auto *taint = taints[*block]; taint && !taint->Full()) {
      TaintedAtoms::Tainted t(*taint);
      for (auto it = block->begin(); it != block->end(); ) {
        Inst *inst = &*it++;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void PreEvalPass::Run(Prog *prog)
{
  if (Func *f = ::dyn_cast_or_null<Func>(prog->GetGlobal("main"))) {
    TaintedAtoms taints(*f);
    for (Func &func : *prog) {
      SimplifyFunction(taints, func);
    }
  }
}

// -----------------------------------------------------------------------------
const char *PreEvalPass::GetPassName() const
{
  return "Partial Pre-Evaluation";
}
