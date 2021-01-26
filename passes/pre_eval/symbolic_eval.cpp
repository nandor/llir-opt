// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>
#include <llvm/Support/Format.h>

#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
bool SymbolicEval::Evaluate()
{
  LLVM_DEBUG(llvm::dbgs() << inst_ << '\n');

  bool changed = Dispatch(inst_);

  #ifndef NDEBUG
    for (unsigned i = 0, n = inst_.GetNumRets(); i < n; ++i) {
      if (auto value = frame_.FindOpt(inst_.GetSubValue(i))) {
        LLVM_DEBUG(llvm::dbgs() << "\t\t" << i << ": " << *value << '\n');
      }
    }
  #endif

  return changed;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitInst(Inst &i)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
ID<SymbolicFrame> SymbolicEval::GetFrame()
{
  return frame_.GetIndex();
}

// -----------------------------------------------------------------------------
SymbolicValue::Origin SymbolicEval::GetOrigin()
{
  return std::make_pair(GetFrame(), &inst_);
}
