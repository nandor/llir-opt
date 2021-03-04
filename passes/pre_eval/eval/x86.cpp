// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"



// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_OutInst(X86_OutInst &i)
{
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_LidtInst(X86_LidtInst &i)
{
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_LgdtInst(X86_LgdtInst &i)
{
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_LtrInst(X86_LtrInst &i)
{
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_WrMsrInst(X86_WrMsrInst &i)
{
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_RdTscInst(X86_RdTscInst &i)
{
  return SetScalar();
}
