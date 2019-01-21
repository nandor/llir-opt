// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <iostream>
#include <cstdlib>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>

#include "core/parser.h"
#include "core/pass_manager.h"
#include "emitter/x86/x86emitter.h"
#include "passes/dead_code_elim.h"
#include "passes/higher_order.h"
#include "passes/inliner.h"
#include "passes/move_elim.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;



// -----------------------------------------------------------------------------
static cl::opt<bool>
kVerbose("v", cl::desc("verbosity flag"), cl::Hidden);

static cl::opt<std::string>
kInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
kOutput("o", cl::desc("output"), cl::init("-"));

static cl::opt<bool>
kOptimise("opt", cl::desc("enable optimisations"));


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
    Parser parser(kInput);
    if (auto *prog = parser.Parse()) {
      // Create a pipeline to optimise the code.
      PassManager passMngr(kVerbose);
      passMngr.Add(new MoveElimPass());
      passMngr.Add(new DeadCodeElimPass());
      if (kOptimise) {
        passMngr.Add(new InlinerPass());
        passMngr.Add(new DeadCodeElimPass());
        passMngr.Add(new HigherOrderPass());
      }
      passMngr.Run(prog);

      // Determine the output type.
      llvm::StringRef out = kOutput;
      bool isBinary;
      if (out.endswith(".S") || out.endswith(".s") || out == "-") {
        isBinary = false;
      } else if (out.endswith(".o")) {
        isBinary = true;
      } else {
        llvm::errs() << "[Error] Invalid output format!\n";
        return EXIT_FAILURE;
      }

      // Open the output stream.
      std::error_code err;
      sys::fs::OpenFlags flags = isBinary ? sys::fs::F_Text : sys::fs::F_None;
      auto output = std::make_unique<llvm::ToolOutputFile>(kOutput, err, flags);
      if (err) {
        llvm::errs() << err.message() << "\n";
        return EXIT_FAILURE;
      }

      // Generate code.
      if (isBinary) {
        X86Emitter(kInput, output->os()).EmitOBJ(prog);
      } else {
        X86Emitter(kInput, output->os()).EmitASM(prog);
      }

      output->keep();
    }
    return EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    std::cerr << "[Exception] " << ex.what() << "\n";
    return EXIT_FAILURE;
  }
}
