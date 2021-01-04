// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"



// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_OutInst(X86_OutInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_LidtInst(X86_LidtInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_LgdtInst(X86_LgdtInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_LtrInst(X86_LtrInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_SetCsInst(X86_SetCsInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_SetDsInst(X86_SetDsInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_WrMsrInst(X86_WrMsrInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_RdTscInst(X86_RdTscInst &i)
{
  return ctx_.Set(i, SymbolicValue::Scalar());
}
