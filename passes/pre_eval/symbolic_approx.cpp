// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>

#include "passes/pre_eval/reference_graph.h"
#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
void SymbolicApprox::Approximate(std::vector<Block *> blocks)
{
  bool changed;
  do {
    changed = false;
    for (Block *block : blocks) {
      // Evaluate all instructions in the block, except the terminator.
      for (auto it = block->begin(); std::next(it) != block->end(); ++it) {
        if (auto *phi = ::cast_or_null<PhiInst>(&*it)) {
          std::optional<SymbolicValue> value;
          for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
            if (auto *phiValue = ctx_.FindOpt(phi->GetValue(i))) {
              if (value) {
                value = value->LUB(*phiValue);
              } else {
                value = *phiValue;
              }
            }
          }
          assert(value && "block not yet reached");
          LLVM_DEBUG(llvm::dbgs() << *it << "\n\t0: " << *value << '\n');
          changed = ctx_.Set(*phi, *value) || changed;
        } else {
          changed = SymbolicEval(ctx_).Evaluate(*it) || changed;
        }
      }

      // Evaluate the terminator if it is a call.
      auto *term = block->GetTerminator();
      switch (term->GetKind()) {
        default: llvm_unreachable("invalid terminator");
        case Inst::Kind::JUMP:
        case Inst::Kind::JUMP_COND:
        case Inst::Kind::TRAP: {
          break;
        }
        case Inst::Kind::CALL:
        case Inst::Kind::TAIL_CALL:
        case Inst::Kind::INVOKE: {
          llvm_unreachable("not implemented");
        }
        case Inst::Kind::RAISE: {
          llvm_unreachable("not implemented");
        }
      }
    }
  } while (changed);
}

// -----------------------------------------------------------------------------
void SymbolicApprox::Approximate(CallSite &call)
{
  if (auto *func = call.GetDirectCallee()) {
    auto &node = refs_.FindReferences(*func);
    if (node.HasIndirectCalls || node.HasRaise) {
      llvm_unreachable("not implemented");
    } else {
      #ifndef NDEBUG
      LLVM_DEBUG(llvm::dbgs() << "\t\tCall to: '" << func->getName() << "'\n");
      for (auto *g : node.Referenced) {
        LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << g->getName() << "\n");
      }
      #endif
      for (auto arg : call.args()) {
        auto argVal = ctx_.Find(arg);
        LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << argVal << "\n");
        switch (argVal.GetKind()) {
          case SymbolicValue::Kind::UNKNOWN_INTEGER:
          case SymbolicValue::Kind::UNDEFINED:
          case SymbolicValue::Kind::INTEGER: {
            continue;
          }
          case SymbolicValue::Kind::UNKNOWN: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::POINTER: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid value kind");
      }
      llvm_unreachable("not implemented");
    }
  } else {
    llvm_unreachable("not implemented");
  }
}
