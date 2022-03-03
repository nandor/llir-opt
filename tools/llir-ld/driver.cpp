// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <sstream>

#include <llvm/BinaryFormat/Magic.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Object/Archive.h>

#include "core/bitcode.h"
#include "core/printer.h"
#include "core/prog.h"
#include "core/util.h"
#include "core/error.h"

#include "driver.h"
#include "options.h"
#include "linker.h"



// -----------------------------------------------------------------------------
llvm::Error WithTemp(
    llvm::StringRef ext,
    std::function<llvm::Error(int, llvm::StringRef)> &&f)
{
  // Write the program to a bitcode file.
  auto tmpOrError = llvm::sys::fs::TempFile::create("/tmp/llir-ld-%%%%%%%" + ext);
  if (!tmpOrError) {
    return tmpOrError.takeError();
  }
  auto &tmp = tmpOrError.get();

  // Run the program on the temp file, which is kept on failure.
  auto status = f(tmp.FD, tmp.TmpName);
  if (auto error = status ? tmp.keep() : tmp.discard()) {
    return error;
  }
  return status;
}

// -----------------------------------------------------------------------------
static OptLevel ParseOptLevel(llvm::opt::Arg *arg)
{
  if (arg) {
    switch (arg->getOption().getID()) {
      case OPT_O0: return OptLevel::O0;
      case OPT_O1: return OptLevel::O1;
      case OPT_O2: return OptLevel::O2;
      case OPT_O3: return OptLevel::O3;
      case OPT_O4: return OptLevel::O4;
      case OPT_Os: return OptLevel::Os;
      default: llvm_unreachable("invalid optimisation level");
    }
  } else {
    return OptLevel::O2;
  }
}

// -----------------------------------------------------------------------------
Driver::Driver(
    const llvm::Triple &triple,
    const llvm::Triple &base,
    llvm::opt::InputArgList &args)
  : llirTriple_(triple)
  , baseTriple_(base)
  , args_(args)
  , output_(Abspath(args.getLastArgValue(OPT_o, "a.out")))
  , shared_(args.hasArg(OPT_shared))
  , static_(args.hasArg(OPT_static))
  , noShared_(args.hasFlag(OPT_Bstatic, OPT_Bdynamic, false))
  , relocatable_(args.hasArg(OPT_relocatable))
  , exportDynamic_(args.hasArg(OPT_export_dynamic))
  , ehFrameHdr_(args.hasFlag(OPT_eh_frame_hdr, OPT_no_eh_frame_hdr, false))
  , targetCPU_(args.getLastArgValue(OPT_mcpu))
  , targetABI_(args.getLastArgValue(OPT_mabi))
  , targetFS_(args.getLastArgValue(OPT_mfs))
  , entry_(args.getLastArgValue(OPT_entry))
  , optLevel_(ParseOptLevel(args.getLastArg(OPT_O_Group)))
  , libraryPaths_(args.getAllArgValues(OPT_library_path))
{
  args.ClaimAllArgs(OPT_nostdlib);
  args.ClaimAllArgs(OPT_gc_sections);
}

// -----------------------------------------------------------------------------
Driver::~Driver()
{
  for (auto &&temp : tempFiles_) {
    consumeError(temp.discard());
  }
}

// -----------------------------------------------------------------------------
enum class FileMagic {
  LLIR,
  ARCHIVE,
  BITCODE,
  OBJECT,
  SHARED_OBJECT,
  BLOB,
  EXPORT_LIST,
};

// -----------------------------------------------------------------------------
FileMagic Identify(llvm::StringRef name, llvm::StringRef buffer)
{
  if (IsLLIRObject(buffer)) {
    return FileMagic::LLIR;
  } else if (name.endswith(".def")) {
    return FileMagic::EXPORT_LIST;
  } else {
    switch (llvm::identify_magic(buffer)) {
      case llvm::file_magic::archive: {
        return FileMagic::ARCHIVE;
      }
      case llvm::file_magic::bitcode: {
        return FileMagic::BITCODE;
      }
      case llvm::file_magic::elf:
      case llvm::file_magic::elf_relocatable: {
        return FileMagic::OBJECT;
      }
      case llvm::file_magic::elf_shared_object: {
        return FileMagic::SHARED_OBJECT;
      }
      case llvm::file_magic::unknown: {
        return FileMagic::BLOB;
      }
      default: {
        llvm::report_fatal_error("unknown input kind");
      }
    }
  }
}

// -----------------------------------------------------------------------------
llvm::Expected<Linker::Archive>
Driver::LoadArchive(llvm::MemoryBufferRef buffer)
{
  // Parse the archive.
  auto libOrErr = llvm::object::Archive::create(buffer);
  if (!libOrErr) {
    return libOrErr.takeError();
  }

  // Decode all LLIR objects, dump the rest to text files.
  llvm::Error err = llvm::Error::success();
  Linker::Archive ar;
  for (auto &child : libOrErr.get()->children(err)) {
    // Get the name.
    auto nameOrErr = child.getName();
    if (!nameOrErr) {
      return nameOrErr.takeError();
    }
    auto name = llvm::sys::path::filename(nameOrErr.get());

    // Get the buffer.
    auto bufferOrErr = child.getBuffer();
    if (!bufferOrErr) {
      return bufferOrErr.takeError();
    }
    auto buffer = bufferOrErr.get();
    if (buffer.empty()) {
      continue;
    }

    // Parse bitcode or write data to a temporary file.
    switch (Identify(name, buffer)) {
      case FileMagic::LLIR: {
        auto prog = BitcodeReader(buffer).Read();
        if (!prog) {
          return MakeError("cannot parse bitcode");
        }
        ar.emplace_back(std::move(prog));
        continue;
      }
      case FileMagic::BITCODE: {
        auto &memBuffer = buffers_.emplace_back(
            llvm::MemoryBuffer::getMemBufferCopy(buffer, name)
        );
        if (!memBuffer) {
          return MakeError("cannot create buffer");
        }
        auto bitcodeOrError = llvm::lto::InputFile::create(*memBuffer);
        if (!bitcodeOrError) {
          return bitcodeOrError.takeError();
        }
        ar.emplace_back(std::move(bitcodeOrError.get()));
        continue;
      }
      case FileMagic::OBJECT: {
        llvm_unreachable("not implemented");
      }
      case FileMagic::BLOB: {
        auto tmpOrError = llvm::sys::fs::TempFile::create(
            "/tmp/obj-" + name + "-%%%%%%%"
        );
        if (!tmpOrError) {
          return tmpOrError.takeError();
        }
        auto &tmp = tmpOrError.get();

        std::error_code ec;
        llvm::raw_fd_ostream os(tmp.TmpName, ec, llvm::sys::fs::OF_None);
        if (ec) {
          return llvm::errorCodeToError(ec);
        }
        os.write(buffer.data(), buffer.size());
        ar.emplace_back(Linker::Unit::Data{ tmp.TmpName });
        tempFiles_.emplace_back(std::move(tmp));
        continue;
      }
      case FileMagic::EXPORT_LIST:
      case FileMagic::SHARED_OBJECT:
      case FileMagic::ARCHIVE: {
        llvm::report_fatal_error("nested archives not supported");
      }
    }
    llvm_unreachable("unknown file type");
  }
  if (err) {
    return std::move(err);
  } else {
    return ar;
  }
}

// -----------------------------------------------------------------------------
llvm::Expected<std::optional<Linker::Archive>>
Driver::TryLoadArchive(const std::string &path)
{
  if (llvm::sys::fs::exists(path)) {
    // Open the file.
    auto fileOrErr = llvm::MemoryBuffer::getFile(path);
    if (auto ec = fileOrErr.getError()) {
      return llvm::errorCodeToError(ec);
    }

    // Load the archive.
    auto buffer = fileOrErr.get()->getMemBufferRef();
    auto modulesOrErr = LoadArchive(buffer);
    if (!modulesOrErr) {
      return modulesOrErr.takeError();
    }

    // Record the files.
    return std::move(*modulesOrErr);
  }
  return std::nullopt;
};

// -----------------------------------------------------------------------------
llvm::Error Driver::Link()
{
  // Collect objects and archives.
  Linker linker(llirTriple_, output_);
  bool wholeArchive = false;
  std::unique_ptr<std::list<Linker::Unit>> group = nullptr;

  // Helper to add an archive.
  auto add = [&, this] (Linker::Archive &&archive) -> llvm::Error
  {
    if (wholeArchive) {
      for (auto &&unit : archive) {
        if (auto err = linker.LinkObject(std::move(unit))) {
          return err;
        }
      }
    } else if (group) {
      for (auto &&unit : archive) {
        group->emplace_back(std::move(unit));
      }
    } else {
      if (auto err = linker.LinkGroup(std::move(archive))) {
        return err;
      }
    }
    return llvm::Error::success();
  };

  for (auto *arg : args_) {
    if (arg->isClaimed()) {
      continue;
    }

    switch (arg->getOption().getID()) {
      case OPT_INPUT: {
        llvm::StringRef path = arg->getValue();
        std::string fullPath = Abspath(path);

        // Open the file.
        auto memBufferOrErr = llvm::MemoryBuffer::getFile(fullPath);
        if (auto ec = memBufferOrErr.getError()) {
          return llvm::errorCodeToError(ec);
        }
        auto memBuffer = memBufferOrErr.get()->getMemBufferRef();
        switch (Identify(fullPath, memBuffer.getBuffer())) {
          case FileMagic::LLIR: {
            // Decode an object.
            auto prog = Parse(memBuffer.getBuffer(), fullPath);
            if (!prog) {
              return MakeError("cannot read object: " + fullPath);
            }
            if (auto err = linker.LinkObject(Linker::Unit(std::move(prog)))) {
              return err;
            }
            continue;
          }
          case FileMagic::ARCHIVE: {
            // Decode an archive.
            auto modulesOrErr = LoadArchive(memBuffer);
            if (!modulesOrErr) {
              return modulesOrErr.takeError();
            }
            if (auto err = add(std::move(modulesOrErr.get()))) {
              return std::move(err);
            }
            continue;
          }
          case FileMagic::BITCODE: {
            auto &buf = buffers_.emplace_back(llvm::MemoryBuffer::getMemBufferCopy(
                memBuffer.getBuffer(),
                memBuffer.getBufferIdentifier()
            ));
            auto bitcodeOrError = llvm::lto::InputFile::create(buf->getMemBufferRef());
            if (!bitcodeOrError) {
              return bitcodeOrError.takeError();
            }
            auto &bitcode = bitcodeOrError.get();
            if (auto err = linker.LinkObject(Linker::Unit(std::move(bitcode)))) {
              return err;
            }
            continue;
          }
          case FileMagic::OBJECT: {
            llvm_unreachable("not implemented");
          }
          case FileMagic::SHARED_OBJECT: {
            // Shared libraries are always in executable form,
            // add them to the list of extern libraries.
            externLibs_.push_back(path.str());
            continue;
          }
          case FileMagic::EXPORT_LIST: {
            // TODO: do not ignore these files.
            continue;
          }
          case FileMagic::BLOB: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("unknown file kind");
      }
      case OPT_library: {
        bool found = false;
        llvm::StringRef name = arg->getValue();
        for (const std::string &libPath : libraryPaths_) {
          llvm::SmallString<128> path(libPath);
          if (name.startswith(":")) {
            llvm::sys::path::append(path, name.substr(1));
            auto fullPath = Abspath(std::string(path));
            if (llvm::StringRef(fullPath).endswith(".a")) {
              auto archiveOrError = TryLoadArchive(fullPath);
              if (!archiveOrError) {
                return archiveOrError.takeError();
              }
              if (auto &archive = archiveOrError.get()) {
                if (auto err = add(std::move(*archive))) {
                  return std::move(err);
                }
                found = true;
                continue;
              }
            }
            if (llvm::sys::fs::exists(fullPath)) {
              // Shared libraries are always in executable form,
              // add them to the list of extern libraries.
              externLibs_.push_back(("-l" + name).str());
              found = true;
              break;
            }
          } else {
            llvm::sys::path::append(path, "lib" + name);
            auto fullPath = Abspath(std::string(path));

            if (!static_ && !noShared_) {
              std::string pathSO = fullPath + ".so";
              if (llvm::sys::fs::exists(pathSO)) {
                // Shared libraries are always in executable form,
                // add them to the list of extern libraries.
                externLibs_.push_back(("-l" + name).str());
                found = true;
                break;
              }
            }

            auto archiveOrError = TryLoadArchive(fullPath + ".a");
            if (!archiveOrError) {
              return archiveOrError.takeError();
            }
            if (auto &archive = archiveOrError.get()) {
              if (auto err = add(std::move(*archive))) {
                return std::move(err);
              }
              found = true;
              continue;
            }
          }
        }

        if (!found) {
          return MakeError("cannot find library " + name);
        }
        continue;
      }
      case OPT_whole_archive: {
        wholeArchive = true;
        continue;
      }
      case OPT_no_whole_archive: {
        wholeArchive = false;
        continue;
      }
      case OPT_start_group: {
        if (!group) {
          group.reset(new std::list<Linker::Unit>());
          continue;
        } else {
          return MakeError("nested --start-group");
        }
      }
      case OPT_end_group: {
        if (group) {
          if (auto err = linker.LinkGroup(std::move(*group))) {
            return err;
          }
          group.reset(nullptr);
          continue;
        } else {
          return MakeError("unopened --end-group");
        }
      }
      case OPT_undefined: {
        if (auto err = linker.LinkUndefined(arg->getValue())) {
          return std::move(err);
        }
        continue;
      }
      default: {
        arg->render(args_, forwarded_);
        continue;
      }
    }
  }
  if (group) {
    return MakeError("--start-group not closed");
  }

  // Link the objects together.
  auto progOrError = linker.Link();
  if (!progOrError) {
    return progOrError.takeError();
  }
  auto &&[prog, files] = progOrError.get();
  for (auto &file : files) {
    externLibs_.push_back(file);
  }
  return Output(GetOutputType(), *prog);
}

// -----------------------------------------------------------------------------
Driver::OutputType Driver::GetOutputType()
{
  llvm::StringRef o(output_);
  if (relocatable_) {
    return OutputType::LLBC;
  } else if (o.endswith(".S") || o.endswith(".s")) {
    return OutputType::ASM;
  } else if (o.endswith(".o")) {
    return OutputType::OBJ;
  } else if (o.endswith(".llir")) {
    return OutputType::LLIR;
  } else if (o.endswith(".llbc")) {
    return OutputType::LLBC;
  } else {
    return OutputType::EXE;
  }
}

// -----------------------------------------------------------------------------
llvm::Error Driver::Output(OutputType type, Prog &prog)
{
  switch (type) {
    case OutputType::LLIR: {
      // Write the llir output.
      std::error_code err;
      auto output = std::make_unique<llvm::ToolOutputFile>(
          output_,
          err,
          llvm::sys::fs::F_None
      );
      if (err) {
        return llvm::errorCodeToError(err);
      }

      Printer(output->os()).Print(prog);
      output->keep();
      return llvm::Error::success();
    }
    case OutputType::LLBC: {
      // Write the llbc output.
      std::error_code err;
      auto output = std::make_unique<llvm::ToolOutputFile>(
          output_,
          err,
          llvm::sys::fs::F_None
      );
      if (err) {
        return llvm::errorCodeToError(err);
      }

      BitcodeWriter(output->os()).Write(prog);
      output->keep();
      return llvm::Error::success();
    }
    case OutputType::EXE:
    case OutputType::OBJ:
    case OutputType::ASM: {
      // Lower the final program to the desired format.
      return WithTemp(".llbc", [&](int fd, llvm::StringRef llirPath) {
        {
          llvm::raw_fd_ostream os(fd, false);
          BitcodeWriter(os).Write(prog);
        }

        if (type != OutputType::EXE) {
          return RunOpt(llirPath, output_, type);
        } else {
          return WithTemp(".o", [&](int, llvm::StringRef elfPath) {
            auto error = RunOpt(llirPath, elfPath, OutputType::OBJ);
            if (error) {
              return error;
            }

            const std::string ld = baseTriple_.str() + "-ld";
            std::vector<llvm::StringRef> args;
            args.push_back(ld);

            // Exception handling frames.
            if (ehFrameHdr_) {
              args.push_back("--eh-frame-hdr");
            } else {
              args.push_back("--no-eh-frame-hdr");
            }

            // Common flags.
            args.push_back("-nostdlib");
            // Output file.
            args.push_back("-o");
            args.push_back(output_);
            // Entry point.
            if (!entry_.empty()) {
              args.push_back("-e");
              args.push_back(entry_);
            }
            // Forwarded arguments.
            for (const auto &forwarded : forwarded_) {
              args.push_back(forwarded);
            }
            // Link the inputs.
            args.push_back("--start-group");
            // LLIR-to-ELF code.
            args.push_back(elfPath);
            // Library paths.
            if (!externLibs_.empty()) {
              for (llvm::StringRef lib : libraryPaths_) {
                args.push_back("-L");
                args.push_back(lib);
              }
              // External libraries.
              for (llvm::StringRef lib : externLibs_) {
                args.push_back(lib);
              }
            }
            // Extern objects.
            args.push_back("--end-group");
            if (shared_) {
              // Shared library options.
              args.push_back("-shared");
            } else {
              // Executable options.
              if (static_) {
                // Static executable options.
                args.push_back("-static");
              } else {
                // Dynamic executable options.
                if (exportDynamic_) {
                  args.push_back("-E");
                }
              }
            }

            // Run the linker.
            return RunExecutable(ld, args);
          });
        }
      });
    }
  }
  llvm_unreachable("invalid output type");
}

// -----------------------------------------------------------------------------
llvm::Error Driver::RunOpt(
    llvm::StringRef input,
    llvm::StringRef output,
    OutputType type)
{
  std::string toolName = llirTriple_.str() + "-opt";
  std::vector<llvm::StringRef> args;
  args.push_back(toolName);
  if (auto *opt = getenv("LLIR_OPT_O")) {
    args.push_back(opt);
  } else {
    switch (optLevel_) {
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
  } else if (!targetCPU_.empty()) {
    args.push_back("-mcpu");
    args.push_back(targetCPU_);
  }
  // -mabi
  if (auto *abi = getenv("LLIR_OPT_ABI")) {
    args.push_back("-mabi");
    args.push_back(abi);
  } else if (!targetABI_.empty()) {
    args.push_back("-mabi");
    args.push_back(targetABI_);
  }
  // -mfs
  if (auto *abi = getenv("LLIR_OPT_FS")) {
    args.push_back("-mfs");
    args.push_back(abi);
  } else if (!targetFS_.empty()) {
    args.push_back("-mfs");
    args.push_back(targetFS_);
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
  if (shared_) {
    args.push_back("-shared");
  }
  if (static_) {
    args.push_back("-static");
  }
  if (!entry_.empty()) {
    args.push_back("-entry");
    args.push_back(entry_);
  }
  args.push_back("-emit");
  switch (type) {
    case OutputType::EXE: args.push_back("obj"); break;
    case OutputType::OBJ: args.push_back("obj"); break;
    case OutputType::ASM: args.push_back("asm"); break;
    case OutputType::LLIR: args.push_back("llir"); break;
    case OutputType::LLBC: args.push_back("llbc"); break;
  }

  // Save the IR blob and arguments if requested.
  if (auto *savePath = getenv("LLIR_LD_SAVE")) {
    for (unsigned i = 0; ; ++i) {
      std::string name(llvm::sys::path::filename(input));
      llvm::raw_string_ostream os(name);
      os << "." << i << ".llbc";

      llvm::SmallString<256> path(savePath);
      llvm::sys::path::append(path, name);

      {
        std::error_code ec;
        llvm::raw_fd_ostream os(path, ec, llvm::sys::fs::CD_CreateNew);
        if (ec) {
          if (ec != std::errc::file_exists) {
            return llvm::errorCodeToError(ec);
          } else {
            continue;
          }
        }
      }

      auto ec = llvm::sys::fs::copy_file(input, path);
      if (ec) {
        return llvm::errorCodeToError(ec);
      }
      break;
    }
  }
  return RunExecutable(toolName, args);
}

// -----------------------------------------------------------------------------
llvm::Error Driver::RunExecutable(
    llvm::StringRef exe,
    llvm::ArrayRef<llvm::StringRef> args)
{
  if (auto P = llvm::sys::findProgramByName(exe)) {
    if (auto code = llvm::sys::ExecuteAndWait(*P, args)) {
      std::string str;
      llvm::raw_string_ostream os(str);
      os << "command failed: " << exe << " ";
      for (size_t i = 1, n = args.size(); i < n; ++i) {
        os << args[i] << " ";
      }
      os << "\n";
      return MakeError(str);
    }
    return llvm::Error::success();
  }
  return MakeError("missing executable " + exe);
}
