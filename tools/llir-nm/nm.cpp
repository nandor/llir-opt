// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Object/ArchiveWriter.h>

#include "core/bitcode.h"
#include "core/parser.h"
#include "core/printer.h"
#include "core/prog.h"
#include "core/util.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;


// -----------------------------------------------------------------------------
static llvm::StringRef ToolName;

// -----------------------------------------------------------------------------
static void exitIfError(llvm::Error e, llvm::Twine ctx)
{
  if (!e) {
    return;
  }

  llvm::handleAllErrors(std::move(e), [&](const llvm::ErrorInfoBase &e) {
    llvm::WithColor::error(llvm::errs(), ToolName)
        << ctx << ": " << e.message() << "\n";
  });
  exit(EXIT_FAILURE);
}

// -----------------------------------------------------------------------------
static cl::opt<std::string>
optInput(cl::Positional, cl::desc("<input>"), cl::Required);

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  ToolName = argc > 0 ? argv[0] : "llir-nm";
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLBC dumper\n\n")) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
