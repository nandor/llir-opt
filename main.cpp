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
#include "core/printer.h"
#include "core/pass_manager.h"
#include "emitter/x86/x86emitter.h"
#include "passes/dead_code_elim.h"
#include "passes/dead_func_elim.h"
#include "passes/global_data_elim.h"
#include "passes/higher_order.h"
#include "passes/inliner.h"
#include "passes/move_elim.h"
#include "passes/sccp.h"
#include "passes/simplify_cfg.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;


// -----------------------------------------------------------------------------
enum class OutputType {
  OBJ,
  ASM,
  GENM
};

// -----------------------------------------------------------------------------
static cl::opt<bool>
kVerbose("v", cl::desc("verbosity flag"), cl::Hidden);

static cl::opt<std::string>
kInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
kOutput("o", cl::desc("output"), cl::init("-"));

static cl::opt<bool>
kOptimise("opt", cl::desc("enable optimisations"));

static cl::opt<bool>
kTime("time", cl::desc("time passes"));



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
      PassManager passMngr(kVerbose, kTime);
      passMngr.Add(new MoveElimPass());
      passMngr.Add(new DeadCodeElimPass());
      passMngr.Add(new SimplifyCfgPass());
      if (kOptimise) {
        passMngr.Add(new InlinerPass());
        passMngr.Add(new SCCPPass());
        passMngr.Add(new DeadCodeElimPass());
        passMngr.Add(new DeadFuncElimPass());
        passMngr.Add(new SimplifyCfgPass());
        passMngr.Add(new GlobalDataElimPass());
        passMngr.Add(new HigherOrderPass());
        passMngr.Add(new DeadFuncElimPass());
      }
      passMngr.Run(prog);

      // Determine the output type.
      llvm::StringRef out = kOutput;
      OutputType type;
      bool isBinary;
      if (out.endswith(".S") || out.endswith(".s") || out == "-") {
        type = OutputType::ASM;
        isBinary = false;
      } else if (out.endswith(".o")) {
        type = OutputType::OBJ;
        isBinary = true;
      } else if (out.endswith(".genm")) {
        type = OutputType::GENM;
        isBinary = false;
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
      switch (type) {
        case OutputType::ASM: {
          X86Emitter(kInput, output->os()).EmitASM(prog);
          break;
        }
        case OutputType::OBJ: {
          X86Emitter(kInput, output->os()).EmitOBJ(prog);
          break;
        }
        case OutputType::GENM: {
          Printer(output->os()).Print(prog);
          break;
        }
      }

      output->keep();
    }
    return EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    std::cerr << "[Exception] " << ex.what() << "\n";
    return EXIT_FAILURE;
  }
}
