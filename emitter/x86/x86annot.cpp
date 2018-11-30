// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>

#include "emitter/x86/x86annot.h"



// -----------------------------------------------------------------------------
char X86Annot::ID;



// -----------------------------------------------------------------------------
X86Annot::X86Annot(const X86ISel *isel, const Prog *prog)
  : llvm::ModulePass(ID)
  , isel_(isel)
  , prog_(prog)
{
}

// -----------------------------------------------------------------------------
bool X86Annot::runOnModule(llvm::Module &M)
{
  llvm::errs() << "X86Annot\n";
  return false;
}

// -----------------------------------------------------------------------------
llvm::StringRef X86Annot::getPassName() const
{
  return "GenM X86 Annotation Inserter";
}

// -----------------------------------------------------------------------------
void X86Annot::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.addRequired<llvm::MachineModuleInfo>();
  AU.addPreserved<llvm::MachineModuleInfo>();
}
