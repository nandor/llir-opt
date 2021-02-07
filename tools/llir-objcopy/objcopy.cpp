// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <fstream>

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
optKeepGlobalSymbol(
    "keep-global-symbol",
    cl::desc("Symbols to keep as globals")
);
static cl::alias optG(
    "G",
    cl::desc("Alias for --keep-global-symbol"),
    cl::aliasopt(optKeepGlobalSymbol)
);
static cl::list<std::string>
optKeepGlobalSymbols(
    "keep-global-symbols",
    cl::desc("Symbols to keep as globals")
);

static cl::list<std::string>
optLocalizeSymbol(
    "localize-symbol",
    cl::desc("Convert a global to a local")
);
static cl::alias optL(
    "L",
    cl::desc("Alias for --localize-symbol"),
    cl::aliasopt(optLocalizeSymbol)
);
static cl::list<std::string>
optLocalizeSymbols(
    "localize-symbols",
    cl::desc("Symbols to keep as globals")
);

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
static llvm::Expected<std::vector<Global *>>
FindGlobals(
    Prog &p,
    const std::vector<std::string> &symbols,
    const std::vector<std::string> &files)
{
  std::vector<Global *> globals;

  std::set<std::string> names(symbols.begin(), symbols.end());
  for (auto &file : files) {
    std::string g;
    std::ifstream is(file);
    while (std::getline(is, g)) {
      names.emplace(g);
    }
  }

  if (optW) {
    std::vector<llvm::GlobPattern> patterns;
    for (auto &g : names) {
      if (auto pat = llvm::GlobPattern::create(g)) {
        patterns.emplace_back(std::move(*pat));
      } else {
        return std::move(pat.takeError());
      }
    }

    for (auto *g : p.globals()) {
      if (g->IsLocal()) {
        continue;
      }

      bool matched = false;
      for (auto &pat : patterns) {
        if (pat.match(g->getName())) {
          matched = true;
          break;
        }
      }
      if (matched) {
        globals.push_back(g);
      }
    }
  } else {
    for (auto *g : p.globals()) {
      if (g->IsLocal()) {
        continue;
      }
      if (names.count(std::string(g->GetName()))) {
        globals.push_back(g);
      }
    }
  }
  return globals;
}

// -----------------------------------------------------------------------------
bool RunObjcopy(Prog &p)
{
  bool keepGlobal = !optKeepGlobalSymbol.empty() || !optKeepGlobalSymbols.empty();
  bool localize = !optLocalizeSymbol.empty() || !optLocalizeSymbols.empty();
  if (keepGlobal) {
    auto globalsOrErr = FindGlobals(p, optKeepGlobalSymbol, optKeepGlobalSymbols);
    exitIfError(globalsOrErr.takeError(), "cannot identify globals");
    std::set<Global *> matched(globalsOrErr->begin(), globalsOrErr->end());
    for (auto *g : p.globals()) {
      if (!matched.count(g)) {
        g->SetVisibility(Visibility::LOCAL);
      }
    }
    return true;
  }

  if (localize) {
    auto globalsOrErr = FindGlobals(p, optLocalizeSymbol, optLocalizeSymbols);
    exitIfError(globalsOrErr.takeError(), "cannot identify globals");
    for (auto *g : *globalsOrErr) {
      g->SetVisibility(Visibility::LOCAL);
    }
    return true;
  }

  return true;
}


// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  ToolName = (argc == 0 ? "llir-ar" : argv[0]);
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
