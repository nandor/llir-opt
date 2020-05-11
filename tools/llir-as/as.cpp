// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ToolOutputFile.h>

#include "core/bitcode.h"
#include "core/parser.h"
#include "core/prog.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;



// -----------------------------------------------------------------------------
static cl::opt<std::string>
optInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
optOutput("o", cl::desc("output"), cl::init("-"));



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

  // Open the output stream.
  std::error_code err;
  auto output = std::make_unique<llvm::ToolOutputFile>(
      optOutput,
      err,
      sys::fs::F_Text
  );
  if (err) {
    llvm::errs() << "[Error] Cannot open output: " << err.message() << "\n";
    return EXIT_FAILURE;
  }

  // Parse the file.
  auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();
  auto prog = Parser(buffer).Parse();
  if (!prog) {
    return EXIT_FAILURE;
  }

  // Dump the bitcode to the output.
  BitcodeWriter(output->os()).Write(*prog);

  // Success.
  output->keep();
  return EXIT_SUCCESS;
}
