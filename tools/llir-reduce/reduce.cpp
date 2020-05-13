// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ToolOutputFile.h>

#include "core/bitcode.h"
#include "core/parser.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "core/util.h"
#include "passes/dead_code_elim.h"
#include "passes/dead_data_elim.h"
#include "passes/dead_func_elim.h"
#include "passes/move_elim.h"
#include "passes/reduce.h"
#include "passes/sccp.h"
#include "passes/simplify_cfg.h"
#include "passes/stack_object_elim.h"
#include "passes/undef_elim.h"
#include "passes/verifier.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;



// -----------------------------------------------------------------------------
static cl::opt<std::string>
optInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
optOutput("o", cl::desc("output"), cl::init("-"));

static cl::opt<unsigned>
optSeed("seed", cl::desc("random seed"), cl::init(0));



// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLIR optimiser\n\n")) {
    return EXIT_FAILURE;
  }

  // Open the input.
  auto FileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(optInput);
  if (auto EC = FileOrErr.getError()) {
    llvm::errs() << "[Error] Cannot open input: " + EC.message();
    return EXIT_FAILURE;
  }

  // Parse the input, alter it and simplify it.
  auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();
  std::unique_ptr<Prog> prog(Parse(buffer, "llir-reduce"));
  if (!prog) {
    return EXIT_FAILURE;
  }

  // Set up a simple pipeline.
  PassManager mngr(false, false);
  mngr.Add<VerifierPass>();
  mngr.Add<MoveElimPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<ReducePass>(static_cast<unsigned>(optSeed));
  mngr.Add<SCCPPass>();
  mngr.Add<UndefElimPass>();
  mngr.Add<SimplifyCfgPass>();
  mngr.Add<MoveElimPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<StackObjectElimPass>();
  mngr.Add<DeadFuncElimPass>();
  mngr.Add<DeadDataElimPass>();
  mngr.Add<VerifierPass>();

  // Run the optimiser and reducer.
  mngr.Run(*prog);

  // Open the output stream.
  std::error_code err;
  auto output = std::make_unique<llvm::ToolOutputFile>(
      optOutput,
      err,
      sys::fs::F_Text
  );
  if (err) {
    llvm::errs() << err.message() << "\n";
    return EXIT_FAILURE;
  }

  // Emit the simplified file.
  BitcodeWriter(output->os()).Write(*prog);
  output->keep();
  return EXIT_SUCCESS;
}
