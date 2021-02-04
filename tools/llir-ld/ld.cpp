// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <sstream>

#include <clang/Driver/ToolChain.h>
#include <llvm/ADT/PointerUnion.h>
#include <llvm/Object/Archive.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
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
  /// Slow optimisations.
  O3,
  /// All optimisations.
  O4,
  /// Optimise for size.
  Os
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

/**
 * Hash style options.
 */
enum class HashStyle {
  SYSV,
  GNU,
  BOTH,
};

/**
 * Chosen emulation.
 */
enum class Emulation {
  ELF_X86_64
};

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
optInput(cl::Positional, cl::desc("<input>"), cl::ZeroOrMore);

static cl::opt<std::string>
optOutput("o", cl::desc("output"), cl::init("-"));

static cl::list<std::string>
optLibPaths("L", cl::desc("library path"), cl::Prefix);

static cl::list<std::string>
optLibraries("l", cl::desc("libraries"), cl::Prefix);

static cl::opt<std::string>
optEntry("e", cl::desc("entry point"));

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
    clEnumValN(OptLevel::O3, "O3", "Slow optimisations"),
    clEnumValN(OptLevel::O4, "O4", "All optimisations"),
    clEnumValN(OptLevel::Os, "Os", "Optimise for size")
  ),
  cl::init(OptLevel::O0)
);

static cl::opt<std::string>
optRPath("rpath", cl::desc("runtime path"), cl::ZeroOrMore);

static cl::opt<HashStyle>
optHashStyle("hash-style", cl::desc("hashing style"),
  cl::values(
    clEnumValN(HashStyle::BOTH, "both", "include both hash tables"),
    clEnumValN(HashStyle::GNU, "gnu", "GNU-style hash tables"),
    clEnumValN(HashStyle::SYSV, "sysv", "Classic hash tables")
  ),
  cl::init(HashStyle::BOTH)
);

static cl::opt<std::string>
optSOName(
    "soname",
    cl::desc("override .so name"),
    cl::Optional
);

static cl::opt<std::string>
optVersionScript(
    "version-script",
    cl::desc("provide a version script"),
    cl::Optional
);

static cl::opt<std::string>
optT(
    "T",
    cl::desc("path to a linker script"),
    cl::Optional
);

static cl::opt<std::string>
optCPU("mcpu", cl::desc("Target CPU"));

static cl::opt<std::string>
optABI("mabi", cl::desc("Target ABI"));

static cl::opt<std::string>
optFS("mfs", cl::desc("Target feature string"));

static cl::opt<bool>
optV("v", cl::desc("Print version information"), cl::init(false));

static cl::list<std::string>
optZ("z", cl::desc("Additional keywords"));

static cl::opt<bool>
optNoStdlib("nostdlib", cl::desc("Do not link the standard library"));

static cl::opt<unsigned>
optTTextSegment(
    "Ttext-segment",
    cl::desc("Set text segment address"),
    cl::init(-1)
);

static cl::opt<Emulation>
optEmulation("m", cl::desc("emulate the chosen linker"),
  cl::values(
    clEnumValN(Emulation::ELF_X86_64, "elf_x86_64", "ELF x86_64")
  ),
  cl::Optional,
  cl::Prefix
);

static cl::opt<bool>
optDiscardLocals(
    "X",
    cl::desc("Delete all temporary local symbols"),
    cl::init(false)
);

// -----------------------------------------------------------------------------
int WithTemp(
    llvm::StringRef ext,
    std::function<int(int, llvm::StringRef)> &&f)
{
  // Write the program to a bitcode file.
  auto tmp = llvm::sys::fs::TempFile::create("/tmp/llir-ld-%%%%%%%" + ext);
  exitIfError(tmp.takeError(), "cannot create temporry file");

  // Run the program on the temp file.
  int code = f(tmp->FD, tmp->TmpName);

  // Delete the temp file.
  if (auto error = code ? tmp->keep() : tmp->discard()) {
    llvm::WithColor::error(llvm::errs(), ToolName)
        << "cannot delete temporary file: " << error << "\n";
    return EXIT_FAILURE;
  }
  return code;
}

// -----------------------------------------------------------------------------
static int RunExecutable(
    llvm::StringRef exe,
    llvm::ArrayRef<llvm::StringRef> args)
{
  if (auto P = llvm::sys::findProgramByName(exe)) {
    if (auto code = llvm::sys::ExecuteAndWait(*P, args)) {
      auto &log = llvm::WithColor::error(llvm::errs(), ToolName);
      log << "command failed: " << exe << " ";
      for (size_t i = 1, n = args.size(); i < n; ++i) {
        log << args[i] << " ";
      }
      log << "\n";
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  llvm::WithColor::error(llvm::errs(), ToolName)
      << "missing executable: " << exe << "\n";
  return EXIT_FAILURE;
}

// -----------------------------------------------------------------------------
static int RunOpt(
    const llvm::Triple &triple,
    llvm::StringRef input,
    llvm::StringRef output,
    OutputType type)
{
  std::string toolName = triple.str() + "-opt";
  std::vector<llvm::StringRef> args;
  args.push_back(toolName);
  if (auto *opt = getenv("LLIR_OPT_O")) {
    args.push_back(opt);
  } else {
    switch (optOptLevel) {
      case OptLevel::O0: args.push_back("-O0"); break;
      case OptLevel::O1: args.push_back("-O1"); break;
      case OptLevel::O2: args.push_back("-O2"); break;
      case OptLevel::O3: args.push_back("-O3"); break;
      case OptLevel::O4: args.push_back("-O4"); break;
      case OptLevel::Os: args.push_back("-Os"); break;
    }
  }
  // -mcpu
  if (auto *cpu = getenv("LLIR_OPT_CPU")) {
    args.push_back("-mcpu");
    args.push_back(cpu);
  } else if (!optCPU.empty()) {
    args.push_back("-mcpu");
    args.push_back(optCPU);
  }
  // -mabi
  if (auto *abi = getenv("LLIR_OPT_ABI")) {
    args.push_back("-mabi");
    args.push_back(abi);
  } else if (!optABI.empty()) {
    args.push_back("-mabi");
    args.push_back(optABI);
  }
  // -mfs
  if (auto *abi = getenv("LLIR_OPT_FS")) {
    args.push_back("-mfs");
    args.push_back(abi);
  } else if (!optFS.empty()) {
    args.push_back("-mfs");
    args.push_back(optFS);
  }
  // Additional flags.
  if (auto *flags = getenv("LLIR_OPT_FLAGS")) {
    llvm::SmallVector<llvm::StringRef, 3> tokens;
    llvm::StringRef(flags).split(tokens, " ", -1, false);
    for (llvm::StringRef flag : tokens) {
      args.push_back(flag);
    }
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
  if (!optEntry.empty()) {
    args.push_back("-entry");
    args.push_back(optEntry);
  }
  args.push_back("-emit");
  switch (type) {
    case OutputType::EXE: args.push_back("obj"); break;
    case OutputType::OBJ: args.push_back("obj"); break;
    case OutputType::ASM: args.push_back("asm"); break;
    case OutputType::LLIR: args.push_back("llir"); break;
    case OutputType::LLBC: args.push_back("llbc"); break;
  }
  return RunExecutable(toolName, args);
}

// -----------------------------------------------------------------------------
enum class Result {
  LOADED,
  MISSING,
  FAILED,
};

// -----------------------------------------------------------------------------
using ExternFiles = std::vector<std::pair<llvm::sys::fs::TempFile, std::string>>;

// -----------------------------------------------------------------------------
llvm::Expected<std::vector<std::unique_ptr<Prog>>>
LoadArchive(ExternFiles &externs, llvm::MemoryBufferRef buffer)
{
  // Parse the archive.
  auto libOrErr = llvm::object::Archive::create(buffer);
  if (!libOrErr) {
    return libOrErr.takeError();
  }

  // Decode all LLIR objects, dump the rest to text files.
  llvm::Error err = llvm::Error::success();
  std::vector<std::unique_ptr<Prog>> progs;
  for (auto &child : libOrErr.get()->children(err)) {
    auto bufferOrErr = child.getBuffer();
    if (!bufferOrErr) {
      return std::move(bufferOrErr.takeError());
    }
    auto buffer = bufferOrErr.get();
    if (IsLLIRObject(buffer)) {
      auto prog = BitcodeReader(buffer).Read();
      if (!prog) {
        return llvm::make_error<llvm::StringError>(
            "cannot parse bitcode",
            llvm::inconvertibleErrorCode()
        );
      }
      progs.emplace_back(std::move(prog));
    } else {
      llvm_unreachable("not implemented");
    }
  }
  if (err) {
    return std::move(err);
  } else {
    return progs;
  }
}

// -----------------------------------------------------------------------------
static Result TryLoadArchive(
    const std::string &path,
    ExternFiles &externs,
    std::vector<std::unique_ptr<Prog>> &archives)
{
  if (llvm::sys::fs::exists(path)) {
    // Open the file.
    auto fileOrErr = llvm::MemoryBuffer::getFile(path);
    if (auto EC = fileOrErr.getError()) {
      llvm::WithColor::error(llvm::errs(), ToolName)
          << "cannot open " << path << ": " << EC.message() << "\n";
      return Result::FAILED;
    }

    // Load the archive.
    auto buffer = fileOrErr.get()->getMemBufferRef();
    auto modulesOrErr = LoadArchive(externs, buffer);
    exitIfError(modulesOrErr.takeError(), "cannot load archive");

    // Record the files.
    for (auto &&module : *modulesOrErr) {
      archives.push_back(std::move(module));
    }
    return Result::LOADED;
  }
  return Result::MISSING;
};

// -----------------------------------------------------------------------------
static const char *kHelp = "LLIR linker\n\nllir-ld: supported targets: elf\n";

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  ToolName = argc > 0 ? argv[0] : "llir-ld";
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, kHelp)) {
    return EXIT_FAILURE;
  }

  // Print version information if requested.
  if (optV) {
    llvm::outs() << "llir-ld: GNU ld compatible\n";
    return EXIT_SUCCESS;
  }

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
  llvm::Triple base(triple);
  switch (base.getArch()) {
    case llvm::Triple::llir_aarch64: {
      base.setArch(llvm::Triple::aarch64);
      break;
    }
    case llvm::Triple::llir_x86_64: {
      base.setArch(llvm::Triple::x86_64);
      break;
    }
    case llvm::Triple::llir_riscv64: {
      base.setArch(llvm::Triple::riscv64);
      break;
    }
    case llvm::Triple::llir_ppc64le: {
      base.setArch(llvm::Triple::ppc64le);
      break;
    }
    default: {
      llvm::WithColor::error(llvm::errs(), ToolName)
          << "unknown target '" << triple.str() << "'\n";
      return EXIT_FAILURE;
    }
  }

  // Canonicalise the file name.
  std::string output(Abspath(optOutput));

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
  std::vector<std::unique_ptr<Prog>> objects;
  std::vector<std::unique_ptr<Prog>> archives;
  std::vector<std::string> externLibs;
  ExternFiles externFiles;

  // Load all archives and objects specified on the command line first.
  for (const std::string &path : optInput) {
    auto fullPath = Abspath(path);

    // Open the file.
    auto FileOrErr = llvm::MemoryBuffer::getFile(fullPath);
    if (auto EC = FileOrErr.getError()) {
      llvm::WithColor::error(llvm::errs(), ToolName)
          << "cannot open " << fullPath << ": " << EC.message() << "\n";
      return EXIT_FAILURE;
    }

    auto memBuffer = FileOrErr.get()->getMemBufferRef();
    auto buffer = memBuffer.getBuffer();
    if (IsLLIRObject(buffer)) {
      // Decode an object.
      auto prog = Parse(buffer, fullPath);
      if (!prog) {
        llvm::WithColor::error(llvm::errs(), ToolName)
            << "cannot read object: " << fullPath << "\n";
        return EXIT_FAILURE;
      }
      objects.push_back(std::move(prog));
      continue;
    }
    if (buffer.startswith("!<arch>")) {
      // Decode an archive.
      auto modulesOrErr = LoadArchive(externFiles, memBuffer);
      exitIfError(modulesOrErr.takeError(), "cannot load archive");
      for (auto &&module : modulesOrErr.get()) {
        archives.push_back(std::move(module));
      }
      continue;
    }

    // Forward the input to the linker.
    externLibs.push_back(path);
  }

  // Load archives, looking at search paths.
  for (llvm::StringRef name : optLibraries) {
    bool found = false;

    for (const std::string &libPath : optLibPaths) {
      llvm::SmallString<128> path(libPath);
      if (name.startswith(":")) {
        llvm::sys::path::append(path, name.substr(1));
        auto fullPath = Abspath(std::string(path));
        if (llvm::StringRef(fullPath).endswith(".a")) {
          switch (TryLoadArchive(fullPath, externFiles, archives)) {
            case Result::FAILED: {
              return EXIT_FAILURE;
            }
            case Result::MISSING: {
              break;
            }
            case Result::LOADED: {
              found = true;
              continue;
            }
          }
        }
        if (llvm::sys::fs::exists(fullPath)) {
          // Shared libraries are always in executable form,
          // add them to the list of extern libraries.
          externLibs.push_back(("-l" + name).str());
          found = true;
          break;
        }
      } else {
        llvm::sys::path::append(path, "lib" + name);
        auto fullPath = Abspath(std::string(path));

        if (!optStatic) {
          std::string pathSO = fullPath + ".so";
          if (llvm::sys::fs::exists(pathSO)) {
            // Shared libraries are always in executable form,
            // add them to the list of extern libraries.
            externLibs.push_back(("-l" + name).str());
            found = true;
            break;
          }
        }

        switch (TryLoadArchive(fullPath + ".a", externFiles, archives)) {
          case Result::FAILED: {
            return EXIT_FAILURE;
          }
          case Result::MISSING: {
            break;
          }
          case Result::LOADED: {
            found = true;
            continue;
          }
        }
      }
    }

    if (!found) {
      llvm::WithColor::error(llvm::errs(), ToolName)
          << "cannot find library " << name << "\n";
      return EXIT_FAILURE;
    }
  }

  // Link the objects together.
  auto prog = Linker(
      ToolName,
      std::move(objects),
      std::move(archives),
      optOutput
  ).Link();
  if (!prog) {
    return EXIT_FAILURE;
  }

  switch (type) {
    case OutputType::LLIR: {
      // Write the llir output.
      std::error_code err;
      auto output = std::make_unique<llvm::ToolOutputFile>(
          optOutput,
          err,
          llvm::sys::fs::F_None
      );
      if (err) {
        llvm::WithColor::error(llvm::errs(), ToolName) << err.message() << "\n";
        return EXIT_FAILURE;
      }

      Printer(output->os()).Print(*prog);
      output->keep();
      return EXIT_SUCCESS;
    }
    case OutputType::LLBC: {
      // Write the llbc output.
      std::error_code err;
      auto output = std::make_unique<llvm::ToolOutputFile>(
          optOutput,
          err,
          llvm::sys::fs::F_None
      );
      if (err) {
        llvm::WithColor::error(llvm::errs(), ToolName) << err.message() << "\n";
        return EXIT_FAILURE;
      }

      BitcodeWriter(output->os()).Write(*prog);
      output->keep();
      return EXIT_SUCCESS;
    }
    case OutputType::EXE:
    case OutputType::OBJ:
    case OutputType::ASM: {
      // Lower the final program to the desired format.
      return WithTemp(".llbc", [&](int fd, llvm::StringRef llirPath) {
        {
          llvm::raw_fd_ostream os(fd, false);
          BitcodeWriter(os).Write(*prog);
        }

        if (type != OutputType::EXE) {
          return RunOpt(triple, llirPath, optOutput, type);
        } else {
          return WithTemp(".o", [&](int, llvm::StringRef elfPath) {
            auto code = RunOpt(triple, llirPath, elfPath, OutputType::OBJ);
            if (code) {
              return code;
            }

            const std::string ld = base.str() + "-ld";

            std::vector<llvm::StringRef> args;
            std::vector<std::string> allocated;
            args.push_back(ld);
            // Architecture-specific flags.
            switch (triple.getArch()) {
              case llvm::Triple::x86_64:
              case llvm::Triple::llir_x86_64: {
                args.push_back("--no-ld-generated-unwind-info");
                break;
              }
              case llvm::Triple::aarch64:
              case llvm::Triple::llir_aarch64: {
                break;
              }
              case llvm::Triple::riscv64:
              case llvm::Triple::llir_riscv64: {
                break;
              }
              case llvm::Triple::ppc64le:
              case llvm::Triple::llir_ppc64le: {
                break;
              }
              default: {
                llvm::WithColor::error(llvm::errs(), ToolName)
                    << "unkown target '" << triple.str() << "'\n";
                return EXIT_FAILURE;
              }
            }
            // Common flags.
            args.push_back("-nostdlib");
            // Output file.
            args.push_back("-o");
            args.push_back(optOutput);
            // Entry point.
            if (!optEntry.empty()) {
              args.push_back("-e");
              args.push_back(optEntry);
            }
            // rpath.
            if (!optRPath.empty()) {
              args.push_back("-rpath");
              args.push_back(optRPath);
            }
            // linker script.
            if (!optT.empty()) {
              args.push_back("-T");
              args.push_back(optT);
            }
            // Ttext-segment
            if (optTTextSegment != static_cast<unsigned>(-1)) {
              args.push_back("-Ttext-segment");
              args.push_back(allocated.emplace_back(
                  std::to_string(optTTextSegment)
              ));
            }
            // Link the inputs.
            args.push_back("--start-group");
            args.push_back(elfPath);
            for (llvm::StringRef lib : externLibs) {
              args.push_back(lib);
            }
            // Extern objects.
            for (const auto &[tmp, path] : externFiles) {
              args.push_back(path);
            }
            // Library paths.
            if (!externLibs.empty()) {
              for (llvm::StringRef lib : optLibPaths) {
                args.push_back("-L");
                args.push_back(lib);
              }
            }
            args.push_back("--end-group");
            // Executable options.
            if (optShared) {
              args.push_back("-shared");
              if (!optSOName.empty()) {
                args.push_back("-soname");
                args.push_back(optSOName);
              }
              if (!optVersionScript.empty()) {
                args.push_back("--version-script");
                args.push_back(optVersionScript);
              }
            } else {
              if (optExportDynamic) {
                args.push_back("-E");
              }
              if (optStatic) {
                args.push_back("-static");
              }
              if (!optDynamicLinker.empty()) {
                args.push_back("-dynamic-linker");
                args.push_back(optDynamicLinker);
              }
            }
            return RunExecutable(ld, args);
          });
        }
      });
    }
  }
  llvm_unreachable("invalid output type");
}
