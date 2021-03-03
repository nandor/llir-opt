// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <sstream>

#include <llvm/Support/Host.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/WithColor.h>
#include <llvm/Support/TargetSelect.h>

#include "core/util.h"

#include "driver.h"
#include "linker.h"
#include "options.h"

namespace cl = llvm::cl;



// -----------------------------------------------------------------------------
static llvm::StringRef ToolName;

// -----------------------------------------------------------------------------
static void exitIfError(llvm::Error e, llvm::Twine ctx)
{
  if (!e) {
    return;
  }

  llvm::handleAllErrors(std::move(e), [&](const llvm::ErrorInfoBase &e) {
    auto &os = llvm::WithColor::error(llvm::errs(), ToolName);
    std::string str(ctx.str());
    if (str.empty()) {
      os << e.message() << "\n";
    } else {
      os << str << ": " << e.message() << "\n";
    }
  });
  exit(EXIT_FAILURE);
}

// -----------------------------------------------------------------------------
static const char *kHelp = "LLIR linker\n\nllir-ld: supported targets: elf\n";


// -----------------------------------------------------------------------------
static llvm::Expected<std::pair<llvm::Triple, llvm::Triple>>
getTriple(llvm::StringRef tool)
{
  // Find the program name and triple.
  llvm::Triple triple;
  {
    const std::string &t = ParseToolName(ToolName, "ld");
    if (t.empty()) {
      triple = llvm::Triple(llvm::sys::getDefaultTargetTriple());
    } else {
      triple = llvm::Triple(t);
    }
  }

  // Find the base triple (non-LLIR version).
  return std::make_pair(triple, triple.getNativeVariant());
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  // Initialise the relevant LLVM modules.
  llvm::InitLLVM X(argc, argv);
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  ToolName = (argc == 0 ? "llir-ld" : argv[0]);

  // Parse the options.
  OptionTable parser;
  llvm::ArrayRef<char *> argVec(argv, argv + argc);
  auto argsOrError = parser.Parse(argVec.slice(1));
  exitIfError(argsOrError.takeError(), "unknown argument");
  auto &args = argsOrError.get();

  // Handle the help option.
  if (args.hasArg(OPT_help)) {
    parser.PrintHelp(
        llvm::outs(),
        (ToolName + " [options] file...").str().c_str(),
        kHelp,
        false,
        true
    );
    return EXIT_SUCCESS;
  }

  // Handle the version option.
  if (args.hasArg(OPT_v)) {
    llvm::outs() << "llir-ld: GNU ld compatible\n";
    return EXIT_SUCCESS;
  }

  // Get the triple.
  auto tripleOrError = getTriple(ToolName);
  exitIfError(tripleOrError.takeError(), "unknown triple");
  auto &[triple, base] = tripleOrError.get();

  // Run the linker.
  exitIfError(Driver(triple, base, args).Link(), "linking failed");
  return EXIT_SUCCESS;
}
