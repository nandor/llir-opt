// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "emitter/emitter.h"



// -----------------------------------------------------------------------------
Emitter::~Emitter()
{
}


// -----------------------------------------------------------------------------
void Emitter::EmitASM(const Prog *prog)
{
  Emit(llvm::TargetMachine::CGFT_AssemblyFile, prog);
}

// -----------------------------------------------------------------------------
void Emitter::EmitOBJ(const Prog *prog)
{
  Emit(llvm::TargetMachine::CGFT_ObjectFile, prog);
}
