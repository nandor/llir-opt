// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>

#include <llvm/ADT/PointerUnion.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/WithColor.h>

#include "core/bitcode.h"
#include "core/printer.h"
#include "core/prog.h"
#include "core/util.h"

#include "linker.h"

namespace cl = llvm::cl;



/**
 * Enumeration of optimisation levels.
 */
enum class OptLevel {
  /// No optimisations.
  O0,
  /// Simple optimisations.
  O1,
  /// Aggressive optimisations.
  O2,
  /// All optimisations.
  O3
};

/**
 * Enumeration of output formats.
 */
enum class OutputType {
  EXE,
  OBJ,
  ASM,
  LLIR,
  LLBC
};



// -----------------------------------------------------------------------------
static cl::list<std::string>
optInput(cl::Positional, cl::desc("<input>"), cl::OneOrMore);

static cl::opt<std::string>
optOutput("o", cl::desc("output"), cl::init("-"));

static cl::list<std::string>
optLibPaths("L", cl::desc("library path"), cl::Prefix);

static cl::list<std::string>
optLibraries("l", cl::desc("libraries"), cl::Prefix);

static cl::opt<std::string>
optEntry("e", cl::desc("entry point"), cl::init("main"));

static cl::opt<bool>
optExportDynamic("E", cl::init(false), cl::ZeroOrMore);

static cl::opt<bool>
optRelocatable("r", cl::desc("relocatable"));

static cl::opt<bool>
optShared("shared", cl::desc("build a shared library"));

static cl::opt<bool>
optStatic("static", cl::desc("build a static executable"));

static cl::opt<std::string>
optDynamicLinker("dynamic-linker", cl::desc("path to the dynamic linker"));

static cl::opt<OptLevel>
optOptLevel(
  cl::desc("optimisation level:"),
  cl::values(
    clEnumValN(OptLevel::O0, "O0", "No optimizations"),
    clEnumValN(OptLevel::O1, "O1", "Simple optimisations"),
    clEnumValN(OptLevel::O2, "O2", "Aggressive optimisations"),
    clEnumValN(OptLevel::O3, "O3", "All optimisations")
  ),
  cl::init(OptLevel::O0)
);

static cl::opt<std::string>
optRPath("rpath", cl::desc("runtime path"), cl::ZeroOrMore);



// -----------------------------------------------------------------------------
int WithTemp(
    const char *argv0,
    llvm::StringRef ext,
    std::function<int(int, llvm::StringRef)> &&f)
{
  // Write the program to a bitcode file.
  auto tmp = llvm::sys::fs::TempFile::create("/tmp/llir-ld-%%%%%%%" + ext);
  if (!tmp) {
    llvm::WithColor::error(llvm::errs(), argv0)
        << "cannot create temporary file: " << tmp.takeError();
    return EXIT_FAILURE;
  }

  // Run the program on the temp file.
  int code = f(tmp->FD, tmp->TmpName);

  // Delete the temp file.
  if (auto error = code ? tmp->keep() : tmp->discard()) {
    llvm::WithColor::error(llvm::errs(), argv0)
        << "cannot delete temporary file: " << error;
    return EXIT_FAILURE;
  }
  return code;
}

// -----------------------------------------------------------------------------
static int RunExecutable(
    const char *argv0,
    const char *exe,
    llvm::ArrayRef<llvm::StringRef> args)
{
  if (auto P = llvm::sys::findProgramByName(exe)) {
    if (auto code = llvm::sys::ExecuteAndWait(*P, args)) {
      auto &log = llvm::WithColor::error(llvm::errs(), argv0);
      log << "command failed: " << exe << " ";
      for (size_t i = 1, n = args.size(); i < n; ++i) {
        log << args[i] << " ";
      }
      log << "\n";
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  llvm::WithColor::error(llvm::errs(), argv0) << "missing executable: " << exe;
  return EXIT_FAILURE;
}

// -----------------------------------------------------------------------------
static int RunOpt(
    const char *argv0,
    llvm::StringRef input,
    llvm::StringRef output,
    OutputType type)
{
  std::vector<llvm::StringRef> args;
  args.push_back("llir-opt");
  switch (optOptLevel) {
    case OptLevel::O0: args.push_back("-O0"); break;
    case OptLevel::O1: args.push_back("-O1"); break;
    case OptLevel::O2: args.push_back("-O2"); break;
    case OptLevel::O3: args.push_back("-O3"); break;
  }
  args.push_back("-o");
  args.push_back(output);
  args.push_back(input);
  if (optShared) {
    args.push_back("-shared");
  }
  if (optStatic) {
    args.push_back("-static");
  }
  args.push_back("-emit");
  switch (type) {
    case OutputType::EXE: args.push_back("obj"); break;
    case OutputType::OBJ: args.push_back("obj"); break;
    case OutputType::ASM: args.push_back("asm"); break;
    case OutputType::LLIR: args.push_back("llir"); break;
    case OutputType::LLBC: args.push_back("llbc"); break;
  }
  return RunExecutable(argv0, "llir-opt", args);
}

// -----------------------------------------------------------------------------
static bool DumpIR(const char *argv0, Prog &prog) {
  // Dump the IR blob into a folder specified by an env var.
  if (auto *path = getenv("LLIR_LD_DUMP_LLBC")) {
    // Generate a file name.
    std::string filename(abspath(optOutput));
    std::replace(filename.begin(), filename.end(), '/', '_');
    llvm::SmallString<128> llbcPath(path);
    llvm::sys::path::append(llbcPath, filename + ".llbc");

    // Write the llbc output.
    std::error_code err;
    auto output = std::make_unique<llvm::ToolOutputFile>(
        llbcPath,
        err,
        llvm::sys::fs::F_None
    );
    if (err) {
      llvm::WithColor::error(llvm::errs(), argv0) << err.message();
      return false;
    }

    BitcodeWriter(output->os()).Write(prog);
    output->keep();
  }

  return true;
}

// -----------------------------------------------------------------------------
static std::optional<std::vector<std::unique_ptr<Prog>>>
LoadArchive(const std::string &path, llvm::StringRef buffer)
{
  std::vector<std::unique_ptr<Prog>> modules;

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
      return {};
    }
    modules.emplace_back(std::move(prog));
  }

  return { std::move(modules) };
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLIR optimiser\n\n")) {
    return EXIT_FAILURE;
  }

  // Find the program name.
  const char *argv0 = argc > 0 ? argv[0] : "llir-ld";

  // Canonicalise the file name.
  std::string output(abspath(optOutput));

  // Determine the output type.
  OutputType type;
  {
    llvm::StringRef o(output);
    if (optRelocatable) {
      type = OutputType::LLBC;
    } else if (o.endswith(".S") || o.endswith(".s")) {
      type = OutputType::ASM;
    } else if (o.endswith(".o")) {
      type = OutputType::OBJ;
    } else if (o.endswith(".llir")) {
      type = OutputType::LLIR;
    } else if (o.endswith(".llbc")) {
      type = OutputType::LLBC;
    } else {
      type = OutputType::EXE;
    }
  }

  // Determine the entry symbols.
  std::set<std::string> entries;
  if (!optShared && !optRelocatable) {
    entries.insert(optEntry);
  }

  // Load objects and libraries.
  std::vector<std::string> missing;
  std::vector<std::unique_ptr<Prog>> objects;
  std::vector<std::unique_ptr<Prog>> archives;

  // Load all archives and objects specified on the command line first.
  for (const std::string &path : optInput) {
    auto fullPath = abspath(path);

    // Open the file.
    auto FileOrErr = llvm::MemoryBuffer::getFile(fullPath);
    if (auto EC = FileOrErr.getError()) {
      llvm::WithColor::error(llvm::errs(), argv0)
          << "cannot open " << fullPath << ": " << EC.message() << "\n";
      return EXIT_FAILURE;
    }
    auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();

    // Decode an archive.
    if (IsLLARArchive(buffer)) {
      if (auto modules = LoadArchive(fullPath, buffer)) {
        for (auto &&module : *modules) {
          archives.push_back(std::move(module));
        }
        continue;
      }
      llvm::WithColor::error(llvm::errs(), argv0)
          << "cannot read archive: " << fullPath << "\n";
      return EXIT_FAILURE;
    }

    // Decode an object.
    if (IsLLIRObject(buffer)) {
      auto prog = Parse(buffer, fullPath);
      if (!prog) {
        llvm::WithColor::error(llvm::errs(), argv0)
            << "cannot read object: " << fullPath << "\n";
        return EXIT_FAILURE;
      }
      objects.push_back(std::move(prog));
      continue;
    }

    // Forward the input to the linker.
    missing.push_back(fullPath);
  }

  // Load archives, looking at search paths.
  for (const std::string &name : optLibraries) {
    bool found = false;
    for (const std::string &libPath : optLibPaths) {
      llvm::SmallString<128> path(libPath);
      llvm::sys::path::append(path, "lib" + name);
      auto fullPath = abspath(std::string(path));

      if (!optStatic) {
        std::string pathSO = fullPath + ".so";
        if (llvm::sys::fs::exists(pathSO)) {
          // Shared libraries are always in executable form,
          // add them to the list of missing libraries.
          missing.push_back(pathSO);
          found = true;
          break;
        }
      }

      std::string pathA = fullPath + ".a";
      if (llvm::sys::fs::exists(pathA)) {
        // Open the file.
        auto FileOrErr = llvm::MemoryBuffer::getFile(pathA);
        if (auto EC = FileOrErr.getError()) {
          llvm::WithColor::error(llvm::errs(), argv0)
              << "cannot open " << pathA << ": " << EC.message() << "\n";
          return EXIT_FAILURE;
        }
        auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();

        // Load the archive.
        if (IsLLARArchive(buffer)) {
          if (auto modules = LoadArchive(pathA, buffer)) {
            for (auto &&module : *modules) {
              archives.push_back(std::move(module));
            }
            found = true;
            continue;
          }
          llvm::WithColor::error(llvm::errs(), argv0)
              << "cannot read archive: " << pathA << "\n";
          return EXIT_FAILURE;
        } else {
          missing.push_back(pathA);
          found = true;
          break;
        }
      }
    }

    if (!found) {
      llvm::WithColor::error(llvm::errs(), argv0)
          << "cannot find library " << name << "\n";
      return EXIT_FAILURE;
    }
  }

  // Link the objects together.
  auto prog = Linker(argv0).Link(
      objects,
      archives,
      optOutput,
      entries
  );
  if (!prog) {
    return EXIT_FAILURE;
  }

  // Dump the IR if requested.
  if (!DumpIR(argv0, *prog)) {
    return EXIT_FAILURE;
  }

  // Lower the final program to the desired format.
  return WithTemp(argv0, ".llbc", [&](int fd, llvm::StringRef llirPath) {
    {
      llvm::raw_fd_ostream os(fd, false);
      BitcodeWriter(os).Write(*prog);
    }

    if (type != OutputType::EXE) {
      return RunOpt(argv0, llirPath, optOutput, type);
    } else {
      return WithTemp(argv0, ".o", [&](int, llvm::StringRef elfPath) {
        if (auto code = RunOpt(argv0, llirPath, elfPath, OutputType::OBJ)) {
          return code;
        }

        if (optShared) {
          std::vector<llvm::StringRef> args;
          args.push_back("ld");
          args.push_back(elfPath);
          args.push_back("-o");
          args.push_back(optOutput);
          args.push_back("-shared");
          args.push_back("--no-ld-generated-unwind-info");
          return RunExecutable(argv0, "ld", args);
        } else {
          std::vector<llvm::StringRef> args;
          args.push_back("ld");
          args.push_back("-nostdlib");
          args.push_back("--start-group");
          args.push_back(elfPath);
          for (llvm::StringRef lib : missing) {
            args.push_back(lib);
          }
          args.push_back("--end-group");
          args.push_back("-o");
          args.push_back(optOutput);
          if (!optDynamicLinker.empty()) {
            args.push_back("-dynamic-linker");
            args.push_back(optDynamicLinker);
          }
          if (optExportDynamic) {
            args.push_back("-E");
          }
          if (optStatic) {
            args.push_back("-static");
          }
          args.push_back("--no-ld-generated-unwind-info");
          return RunExecutable(argv0, "ld", args);
        }
      });
    }
  });
}
