// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ToolOutputFile.h>

#include "core/parser.h"
#include "core/pass_manager.h"
#include "core/printer.h"
#include "core/prog.h"
#include "core/util.h"
#include "passes/dead_code_elim.h"
#include "passes/dead_func_elim.h"
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

static cl::opt<unsigned>
kSeed("seed", cl::desc("random seed"), cl::init(0));



// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLIR optimiser\n\n")) {
    return EXIT_FAILURE;
  }

  // Open the input.
  auto FileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(kInput);
  if (auto EC = FileOrErr.getError()) {
    llvm::errs() << "[Error] Cannot open input: " + EC.message();
    return EXIT_FAILURE;
  }

  // Parse the input, alter it and simplify it.
  std::unique_ptr<Prog> prog(Parse(FileOrErr.get()->getMemBufferRef()));
  if (!prog) {
    return EXIT_FAILURE;
  }

  // Set up a simple pipeline.
  PassManager mngr(false, false);
  mngr.Add<MoveElimPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<ReducePass>(static_cast<unsigned>(kSeed));
  mngr.Add<MoveElimPass>();
  mngr.Add<SCCPPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<DeadFuncElimPass>();

  // Run the optimiser and reducer.
  mngr.Run(*prog);

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
  Printer(output->os()).Print(*prog);
  output->keep();
  return EXIT_SUCCESS;
}
