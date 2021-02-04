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

static cl::opt<std::string>
optOutput("o", cl::desc("output"), cl::init("-"));



// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  ToolName = argc > 0 ? argv[0] : "llir-dump";
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLBC dumper\n\n")) {
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
    return EXIT_FAILURE;
  }

  // Create a printer.
  Printer p(output->os());

  // Parse the input, alter it and simplify it.
  auto memBufferRef = FileOrErr.get()->getMemBufferRef();
  auto buffer = memBufferRef.getBuffer();
  if (IsLLIRObject(buffer)) {
    std::unique_ptr<Prog> prog(BitcodeReader(buffer).Read());
    if (!prog) {
      return EXIT_FAILURE;
    }
    p.Print(*prog);
  } else if (buffer.startswith("!<arch>")) {
    // Parse the archive.
    auto libOrErr = llvm::object::Archive::create(memBufferRef);
    exitIfError(libOrErr.takeError(), "cannot create archive");
    auto &lib = libOrErr.get();

    // Decode all LLIR objects, dump the rest to text files.
    llvm::Error err = llvm::Error::success();
    std::vector<std::unique_ptr<Prog>> progs;
    for (auto &child : lib->children(err)) {
      // Get the name of the item.
      auto nameOrErr = child.getName();
      exitIfError(nameOrErr.takeError(), "missing name " + optInput);
      llvm::StringRef name = sys::path::filename(nameOrErr.get());

      // Get the contents.
      auto bufferOrErr = child.getBuffer();
      exitIfError(bufferOrErr.takeError(), "missing contents " + name);
      auto buffer = bufferOrErr.get();

      if (IsLLIRObject(buffer)) {
        auto prog = BitcodeReader(buffer).Read();
        if (!prog) {
          llvm::errs() << "[error] Cannot decode: " << name << "\n";
          return EXIT_FAILURE;
        }
        p.Print(*prog);
      } else {
        output->os() << "Item: " << name << "\n";
      }
    }
    exitIfError(std::move(err), "cannot read archive");
  } else {
    llvm::errs() << "[error] Unknown input: " << optInput << "\n";
    return EXIT_FAILURE;
  }
  output->keep();
  return EXIT_SUCCESS;
}
