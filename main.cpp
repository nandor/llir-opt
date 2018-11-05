// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <iostream>
#include <cstdlib>
#include <llvm/Support/TargetSelect.h>
#include "core/context.h"
#include "core/parser.h"
#include "core/printer.h"
#include "emitter/x86/x86emitter.h"



// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  // Initialise LLVM.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  // Parse the linked blob, optimise it and emit code.
  try {
    Context ctx;
    Parser parser(ctx, argv[1]);
    if (auto *prog = parser.Parse()) {
      X86Emitter(argv[2]).Emit(prog);
    }
    return EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    std::cerr << "[Exception] " << ex.what() << "\n";
    return EXIT_FAILURE;
  }
}
