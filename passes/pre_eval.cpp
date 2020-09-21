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
#include "passes/pre_eval/tainted_objects.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/flow_graph.h"


// -----------------------------------------------------------------------------
const char *PreEvalPass::kPassID = "pre-eval";

#include "core/printer.h"
Printer p(llvm::errs());

// -----------------------------------------------------------------------------
void PreEvalPass::Run(Prog *prog)
{
  llvm::errs() << "pre-eval\n";
  if (Func *f = ::dyn_cast_or_null<Func>(prog->GetGlobal("main"))) {
    TaintedObjects taints(*f);
    for (Func &func : *prog) {
      //p.Print(func);
      if (!func.getName().endswith("__entry")) {
        //continue;
      }
      llvm::errs() << func.getName() << ":\n";
      llvm::ReversePostOrderTraversal<Func *> rpot(&func);

      SymbolicContext context;
      for (Block *block : rpot) {
        for (Inst &inst : *block) {
          if (auto taint = taints[inst]) {
            unsigned n = 0;
            for (auto obj : taint->objects()) {
              ++n;
            }
            llvm::errs() << "\t" << block->getName() << " " << n << "\n";
            //p.Print(inst);
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *PreEvalPass::GetPassName() const
{
  return "Partial Pre-Evaluation";
}
