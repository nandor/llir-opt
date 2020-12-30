// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>

#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
void SymbolicApprox::Approximate(std::vector<Block *> blocks)
{
  // Special case short loops with basic instructions and no calls.
  if (blocks.size() == 1) {
    Block *block = blocks[0];
    switch (block->GetTerminator()->GetKind()) {
      default: {
        llvm_unreachable("not a terminator");
      }
      case Inst::Kind::RAISE:
      case Inst::Kind::RETURN:
      case Inst::Kind::TAIL_CALL:
      case Inst::Kind::TRAP: {
        llvm_unreachable("not a loop");
      }
      case Inst::Kind::CALL:
      case Inst::Kind::INVOKE: {
        break;
      }
      case Inst::Kind::JUMP:
      case Inst::Kind::JUMP_COND:
      case Inst::Kind::SWITCH: {
        return ApproximateShortLoop(block);
      }
    }
  }

  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicApprox::ApproximateShortLoop(Block *block)
{
  bool changed;
  do {
    changed = false;
    for (auto it = block->begin(); it != block->end(); ++it) {
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
        changed = SymbolicEval(ctx_, heap_).Evaluate(*it) || changed;
      }
    }
  } while (changed);
}
