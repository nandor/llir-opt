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

//#include "core/printer.h"
//Printer p(llvm::errs());

// -----------------------------------------------------------------------------
void PreEvalPass::Run(Prog *prog)
{
  FlowGraph g(*prog);
  llvm::errs() << "pre-eval\n";
  if (Func *f = ::dyn_cast_or_null<Func>(prog->GetGlobal("main"))) {
    TaintedObjects taints(*f);
    for (Func &func : *prog) {
      llvm::errs() << func.getName() << ":\n";
      llvm::ReversePostOrderTraversal<Func *> rpot(&func);

      SymbolicContext context;
      for (Block *block : rpot) {
        if (auto taint = taints[*block]) {
          unsigned n = 0;
          for (auto obj : taint->objects()) {
            ++n;
          }
          llvm::errs() << "\t" << block->getName() << " " << n << "\n";

          /*
          // First pass: evaluate all instructions, propagating taint.
          TaintedObjects::Tainted t(*taint);
          for (Inst &inst : *block) {
            SymbolicEval(context, t).Evaluate(&inst);
          }

          // Second pass: replace instructions with constants.
          for (auto it = block->begin(); it != block->end(); ) {
            Inst *inst = &*it++;
            if (auto *st = ::dyn_cast_or_null<StoreInst>(inst)) {
              if (context.IsStoreFolded(st)) {
                st->eraseFromParent();
              }
            } else if (inst->GetNumRets() > 0) {
              const auto value = context[inst];
              switch (value.GetKind()) {
                case SymbolicValue::Kind::INT: {
                  llvm_unreachable("INT");
                }
                case SymbolicValue::Kind::FLOAT: {
                  llvm_unreachable("FLOAT");
                }
                case SymbolicValue::Kind::ATOM: {
                  llvm_unreachable("ATOM");
                }
                case SymbolicValue::Kind::FUNC: {
                  llvm_unreachable("FUNC");
                }
                case SymbolicValue::Kind::BLOCK: {
                  llvm_unreachable("BLOCK");
                }
                case SymbolicValue::Kind::EXTERN: {
                  llvm_unreachable("EXTERN");
                }
                case SymbolicValue::Kind::UNKNOWN: {
                  llvm_unreachable("UNKNOWN");
                }
                case SymbolicValue::Kind::UNDEFINED: {
                  llvm_unreachable("UNDEFINED");
                }
              }
              llvm_unreachable("invalid symbolic value kind");
            }
          }
          */
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
