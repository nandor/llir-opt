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
static cl::list<std::string>
optInputs(cl::Positional, cl::desc("<input>"), cl::OneOrMore);

// -----------------------------------------------------------------------------
static void DumpSymbols(llvm::raw_ostream &os, const Prog &prog)
{
  os << prog.getName() << ":\n";

  for (const Extern &ext : prog.externs()) {
    os << "                 U " << ext.getName() << "\n";
  }

  for (const Func &func : prog) {
    os << "0000000000000000 T " << func.getName() << "\n";
  }

  for (const Data &data : prog.data()) {
    for (const Object &object : data) {
      for (const Atom &atom : object) {
        os << "0000000000000000 D " << atom.getName() << "\n";
      }
    }
  }
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  ToolName = argc > 0 ? argv[0] : "llir-nm";
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLBC dumper\n\n")) {
    return EXIT_FAILURE;
  }

  for (auto input : optInputs) {
    // Open the input.
    auto FileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(input);
    if (auto EC = FileOrErr.getError()) {
      llvm::errs() << "[Error] Cannot open input: " + EC.message();
      return EXIT_FAILURE;
    }
    auto memBufferRef = FileOrErr.get()->getMemBufferRef();
    auto buffer = memBufferRef.getBuffer();
    if (IsLLIRObject(buffer)) {
      std::unique_ptr<Prog> prog(BitcodeReader(buffer).Read());
      if (!prog) {
        return EXIT_FAILURE;
      }
      DumpSymbols(llvm::outs(), *prog);
    } else {
      llvm_unreachable("not implemented");
    }
  }

  return EXIT_SUCCESS;
}
