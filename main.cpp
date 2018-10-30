// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <iostream>
#include <cstdlib>
#include "core/context.h"
#include "core/parser.h"
#include "core/printer.h"
#include "emitter/x86/x86emitter.h"



// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  Context ctx;
  Parser parser(ctx, argv[1]);
  if (auto *prog = parser.Parse()) {
    X86Emitter().Emit(prog);
  }
  return EXIT_SUCCESS;
}
