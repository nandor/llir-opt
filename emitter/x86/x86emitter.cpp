// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/func.h"
#include "core/prog.h"
#include "emitter/x86/x86emitter.h"



// -----------------------------------------------------------------------------
X86Emitter::X86Emitter(const std::string &out)
  : os_(out)
{
}

// -----------------------------------------------------------------------------
X86Emitter::~X86Emitter()
{
}

// -----------------------------------------------------------------------------
void X86Emitter::Emit(const Prog *prog)
{
  os_ << "\t.text" << std::endl;
  for (const auto &func : *prog) {
    Emit(&func);
  }
}

// -----------------------------------------------------------------------------
void X86Emitter::Emit(const Func *func)
{
  os_ << func->GetName() << ":" << std::endl;
}
