// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/GlobPattern.h>

#include "core/bitcode.h"
#include "core/printer.h"
#include "core/prog.h"
#include "core/util.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;



/**
 * Enumeration of output formats.
 */
enum class OutputType {
  LLIR,
  LLBC
};

// -----------------------------------------------------------------------------
static cl::list<std::string>
optFiles(cl::Positional, cl::desc("<input>"),  cl::OneOrMore);

static cl::opt<OutputType>
optEmit("emit", cl::desc("Emit text-based LLIR"),
  cl::values(
    clEnumValN(OutputType::LLIR, "llir", "LLIR text file"),
    clEnumValN(OutputType::LLBC, "llbc", "LLIR binary file")
  ),
  cl::init(OutputType::LLBC)
);

static cl::opt<bool>
optW("w", cl::desc("Allow wildcards in patterns"), cl::init(false));

static cl::list<std::string>
optG("G", cl::desc("Symbols to keep as globals"));



// -----------------------------------------------------------------------------
bool RunObjcopy(Prog &p)
{
  if (optW) {
    std::vector<llvm::GlobPattern> patterns;
    for (auto g : optG) {
      if (auto pat = llvm::GlobPattern::create(g)) {
        patterns.emplace_back(std::move(*pat));
      } else {
        llvm::errs() << llvm::toString(pat.takeError()) << "\n";
      }
    }

    for (auto &f : p) {
      if (f.IsLocal()) {
        continue;
      }

      bool matched = false;
      for (auto &pat : patterns) {
        if (pat.match(f.getName())) {
          matched = true;
          break;
        }
      }

      if (!matched) {
        f.SetVisibility(Visibility::LOCAL);
      }
    }

    return true;
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  const char *argv0 = argc > 0 ? argv[0] : "llir-ld";
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "llir-objcopy")) {
    return EXIT_FAILURE;
  }

  // Open the input.
  auto FileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(optFiles[0]);
  if (auto EC = FileOrErr.getError()) {
    llvm::errs() << "[Error] Cannot open input: " + EC.message();
    return EXIT_FAILURE;
  }

  // Parse the linked blob: if file starts with magic, parse bitcode.
  auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();
  std::unique_ptr<Prog> prog(Parse(buffer, Abspath(optFiles[0])));
  if (!prog) {
    return EXIT_FAILURE;
  }

  // Run the tool.
  if (!RunObjcopy(*prog)) {
    return EXIT_FAILURE;
  }

  // Open the output stream.
  std::error_code err;
  std::string outFile = optFiles.size() > 1 ? optFiles[1] : optFiles[0];
  auto fs = optEmit == OutputType::LLIR ? sys::fs::F_None : sys::fs::F_Text;
  auto output = std::make_unique<llvm::ToolOutputFile>(outFile, err, fs);
  if (err) {
    llvm::errs() << err.message() << "\n";
    return EXIT_FAILURE;
  }

  // Dump the output.
  switch (optEmit) {
    case OutputType::LLIR: {
      Printer(output->os()).Print(*prog);
      break;
    }
    case OutputType::LLBC: {
      BitcodeWriter(output->os()).Write(*prog);
      break;
    }
  }
  output->keep();
  return EXIT_SUCCESS;
}
