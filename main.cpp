// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <iostream>
#include <cstdlib>

#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/InitLLVM.h>

#include "core/context.h"
#include "core/parser.h"
#include "core/printer.h"
#include "emitter/x86/x86emitter.h"

namespace cl = llvm::cl;



// -----------------------------------------------------------------------------
static cl::opt<bool>
kDump("p", cl::desc("Dump assembly"), cl::Hidden);

static cl::opt<std::string>
kInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
kOutput("o", cl::desc("Output Assembly"), cl::value_desc("filename"));


// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "GenM IR compiler\n\n")) {
    return EXIT_FAILURE;
  }

  // Initialise the relevant LLVM modules.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  // Parse the linked blob, optimise it and emit code.
  try {
    Context ctx;
    Parser parser(ctx, kInput);
    if (auto *prog = parser.Parse()) {
      if (kDump) {
        Printer(std::cerr).Print(prog);
      }

      if (!kOutput.empty()) {
        llvm::StringRef out = kOutput;
        if (out.endswith(".S") || out.endswith(".s")) {
          X86Emitter(kOutput).EmitASM(prog);
        } else if (out.endswith(".o")) {
          X86Emitter(kOutput).EmitOBJ(prog);
        } else {
          llvm::errs() << "[Error] Invalid output format!\n";
          return EXIT_FAILURE;
        }
      } else {
        llvm::errs() << "[Error] Missing output filename!\n";
        return EXIT_FAILURE;
      }
    }
    return EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    std::cerr << "[Exception] " << ex.what() << "\n";
    return EXIT_FAILURE;
  }
}
