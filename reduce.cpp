// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/ToolOutputFile.h>

#include "core/parser.h"
#include "core/pass_manager.h"
#include "core/printer.h"
#include "passes/dead_code_elim.h"
#include "passes/move_elim.h"
#include "passes/reduce.h"
#include "passes/sccp.h"
#include "passes/simplify_cfg.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;



// -----------------------------------------------------------------------------
static cl::opt<std::string>
kInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
kOutput("o", cl::desc("output"), cl::init("-"));

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLIR optimiser\n\n")) {
    return EXIT_FAILURE;
  }

  Parser parser(kInput);
  if (auto *prog = parser.Parse()) {
    // Set up a simple pipeline.
    PassManager mngr(false, false);
    mngr.Add<MoveElimPass>();
    mngr.Add<DeadCodeElimPass>();
    mngr.Add<SimplifyCfgPass>();
    mngr.Add<ReducePass>();
    mngr.Add<MoveElimPass>();
    mngr.Add<DeadCodeElimPass>();
    mngr.Add<SimplifyCfgPass>();

    // Run the optimiser and reducer.
    mngr.Run(prog);

    // Open the output stream.
    std::error_code err;
    auto output = std::make_unique<llvm::ToolOutputFile>(
        kOutput,
        err,
        sys::fs::F_Text
    );
    if (err) {
      llvm::errs() << err.message() << "\n";
      return EXIT_FAILURE;
    }

    // Emit the simplified file.
    Printer(output->os()).Print(prog);
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
