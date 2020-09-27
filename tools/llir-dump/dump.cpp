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
#include "core/printer.h"
#include "core/prog.h"
#include "core/util.h"

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
  auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();
  if (IsLLARArchive(buffer)) {
    uint64_t count = ReadData<uint64_t>(buffer, sizeof(uint64_t));
    uint64_t meta = sizeof(uint64_t) + sizeof(uint64_t);
    for (unsigned i = 0; i < count; ++i) {
      size_t size = ReadData<uint64_t>(buffer, meta);
      meta += sizeof(uint64_t);
      uint64_t offset = ReadData<size_t>(buffer, meta);
      meta += sizeof(size_t);

      llvm::StringRef chunk(buffer.data() + offset, size);
      auto prog = BitcodeReader(chunk).Read();
      if (!prog) {
        return EXIT_FAILURE;
      }
      p.Print(*prog);
    }
  } else {
    std::unique_ptr<Prog> prog(BitcodeReader(buffer).Read());
    if (!prog) {
      return EXIT_FAILURE;
    }

    // Emit the output in LLIR format.
    p.Print(*prog);
  }
  output->keep();
  return EXIT_SUCCESS;
}
