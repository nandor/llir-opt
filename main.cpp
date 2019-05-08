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



/**
 * Enumeration of output formats.
 */
enum class OutputType {
  OBJ,
  ASM,
  GENM
};

/**
 * Enumeration of optimisation levels.
 */
enum class OptLevel {
  /// No optimisations.
  O0,
  /// Simple optimisations.
  O1,
  /// Aggressive optimisations.
  O2
};


// -----------------------------------------------------------------------------
static cl::opt<bool>
kVerbose("v", cl::desc("verbosity flag"), cl::Hidden);

static cl::opt<std::string>
kInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
kOutput("o", cl::desc("output"), cl::init("-"));

static cl::opt<bool>
kTime("time", cl::desc("time passes"));

static cl::opt<bool>
kO0("O0", cl::desc("No optimisations"));

static cl::opt<bool>
kO1("O1", cl::desc("Simple optimisations"));

static cl::opt<bool>
kO2("O2", cl::desc("Aggressive optimisations"));



// -----------------------------------------------------------------------------
static OptLevel GetOptLevel()
{
  if (kO2) {
    return OptLevel::O2;
  }
  if (kO1) {
    return OptLevel::O1;
  }
  return OptLevel::O0;
}


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
      PassManager passMngr(kVerbose, kTime);

      // Create a pipeline to optimise the code.
      switch (GetOptLevel()) {
        case OptLevel::O0: {
          passMngr.Add(new MoveElimPass());
          passMngr.Add(new DeadCodeElimPass());
          passMngr.Add(new SimplifyCfgPass());
          break;
        }
        case OptLevel::O1: {
          passMngr.Add(new MoveElimPass());
          passMngr.Add(new DeadCodeElimPass());
          passMngr.Add(new SimplifyCfgPass());
          //passMngr.Add(new InlinerPass());
          //passMngr.Add(new HigherOrderPass());
          //passMngr.Add(new InlinerPass());
          passMngr.Add(new DeadFuncElimPass());
          passMngr.Add(new SCCPPass());
          passMngr.Add(new DeadCodeElimPass());
          passMngr.Add(new SimplifyCfgPass());
          passMngr.Add(new DeadFuncElimPass());
          break;
        }
        case OptLevel::O2: {
          passMngr.Add(new MoveElimPass());
          passMngr.Add(new DeadCodeElimPass());
          passMngr.Add(new SimplifyCfgPass());
          //passMngr.Add(new InlinerPass());
          //passMngr.Add(new HigherOrderPass());
          //passMngr.Add(new InlinerPass());
          passMngr.Add(new DeadFuncElimPass());
          passMngr.Add(new SCCPPass());
          passMngr.Add(new SimplifyCfgPass());
          passMngr.Add(new DeadCodeElimPass());
          passMngr.Add(new GlobalDataElimPass());
          passMngr.Add(new DeadFuncElimPass());
          break;
        }
      }

      // Run the optimiser.
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
