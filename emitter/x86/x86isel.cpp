// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>
#include "emitter/x86/x86isel.h"

using namespace llvm;


char X86ISel::ID;



// -----------------------------------------------------------------------------
bool X86ISel::runOnModule(Module &M)
{
  assert(!"not implemented");
  return true;
}

// -----------------------------------------------------------------------------
StringRef X86ISel::getPassName() const
{
  return "GenM -> DAG pass";
}

// -----------------------------------------------------------------------------
void X86ISel::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<MachineModuleInfo>();
  AU.addPreserved<MachineModuleInfo>();
}
