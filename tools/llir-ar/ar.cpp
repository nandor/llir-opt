// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/WithColor.h>
#include <llvm/Object/ArchiveWriter.h>

#include "core/prog.h"
#include "core/bitcode.h"
#include "core/util.h"

namespace sys = llvm::sys;
using WithColor = llvm::WithColor;



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
static constexpr auto kKind = llvm::object::Archive::K_GNU;

// -----------------------------------------------------------------------------
static int CreateOrUpdateArchive(
    llvm::StringRef path,
    llvm::ArrayRef<llvm::StringRef> objs)
{
  std::unique_ptr<llvm::object::Archive> archive;
  std::unique_ptr<llvm::MemoryBuffer> buffer;
  if (sys::fs::exists(path)) {
    // Open the file.
    auto fileOrErr = llvm::MemoryBuffer::getFile(path);
    if (auto EC = fileOrErr.getError()) {
      llvm::WithColor::error(llvm::errs(), ToolName)
          << "cannot open " << path << ": " << EC.message() << "\n";
      return EXIT_FAILURE;
    }
    auto bufferRef = fileOrErr.get()->getMemBufferRef();

    // Parse the archive.
    auto libOrErr = llvm::object::Archive::create(bufferRef);
    exitIfError(libOrErr.takeError(), "cannot read " + path);

    archive = std::move(libOrErr.get());
    buffer = std::move(fileOrErr.get());
  }

  std::vector<llvm::NewArchiveMember> members;

  // Record old members.
  if (archive) {
    llvm::Error err = llvm::Error::success();
    for (auto &child : archive->children(err)) {
      auto nameOrErr = child.getName();
      exitIfError(nameOrErr.takeError(), "cannot read name " + path);
      auto name = nameOrErr.get();

      bool found = false;
      for (const auto &newName : objs) {
        if (name == newName) {
          found = true;
          break;
        }
      }
      if (!found) {
        auto fileOrErr = llvm::NewArchiveMember::getOldMember(child, false);
        exitIfError(fileOrErr.takeError(), "cannot record " + name);
        members.emplace_back(std::move(fileOrErr.get()));
      }
    }
    exitIfError(std::move(err), "cannot list archive");
  }

  // Find new members.
  for (unsigned i = 0, n = objs.size(); i < n; ++i) {
    bool found = false;
    if (archive) {
      llvm::Error err = llvm::Error::success();
      for (auto &child : archive->children(err)) {
        auto nameOrErr = child.getName();
        exitIfError(nameOrErr.takeError(), "cannot read name " + path);
        auto name = nameOrErr.get();

        if (name == objs[i]) {
          found = true;
          break;
        }
      }

      exitIfError(std::move(err), "cannot update archive");
    }

    if (found) {
      llvm_unreachable("not implemented");
    } else {
      auto fileOrErr = llvm::NewArchiveMember::getFile(objs[i], true);
      exitIfError(fileOrErr.takeError(), "cannot open " + objs[i]);
      members.emplace_back(std::move(fileOrErr.get()));
    }
  }

  auto writeErr = llvm::writeArchive(
      path,
      members,
      false,
      kKind,
      true,
      false,
      std::move(buffer)
  );
  exitIfError(std::move(writeErr), "cannot write archive");
  return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------
static int ExtractArchive(const std::string &path, bool verbose)
{
  // Open the file.
  auto fileOrErr = llvm::MemoryBuffer::getFile(path);
  if (auto EC = fileOrErr.getError()) {
    llvm::WithColor::error(llvm::errs(), ToolName)
        << "cannot open " << path << ": " << EC.message() << "\n";
    return EXIT_FAILURE;
  }

  // Parse the archive.
  auto buffer = fileOrErr.get()->getMemBufferRef();
  auto libOrErr = llvm::object::Archive::create(buffer);
  exitIfError(libOrErr.takeError(), "cannot read " + path);

  // Decode all LLIR objects, dump the rest to text files.
  llvm::Error err = llvm::Error::success();
  std::vector<std::unique_ptr<Prog>> progs;
  for (auto &child : libOrErr.get()->children(err)) {
    // Get the name of the item.
    auto nameOrErr = child.getName();
    exitIfError(nameOrErr.takeError(), "missing name " + path);
    llvm::StringRef path = sys::path::filename(nameOrErr.get());

    // Get the contents of the data item.
    auto bufferOrError = child.getBuffer();
    exitIfError(bufferOrError.takeError(), "missing data" + path);
    llvm::StringRef buffer = bufferOrError.get();

    // Write to the output.
    std::error_code ec;
    llvm::raw_fd_ostream os(path.str(), ec, llvm::sys::fs::OF_None);
    if (ec) {
      llvm::WithColor::error(llvm::errs(), ToolName)
          << "cannot open " << path << ": " << ec.message() << "\n";
      return EXIT_FAILURE;
    }
    os.write(buffer.data(), buffer.size());
  }

  if (err) {
    llvm::WithColor::error(llvm::errs(), ToolName)
        << "cannot list archive '" << path << "': " << err << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------
static int ListArchive(const std::string &path)
{
  // Open the file.
  auto fileOrErr = llvm::MemoryBuffer::getFile(path);
  if (auto EC = fileOrErr.getError()) {
    llvm::WithColor::error(llvm::errs(), ToolName)
        << "cannot open " << path << ": " << EC.message() << "\n";
    return EXIT_FAILURE;
  }

  // Parse the archive.
  auto buffer = fileOrErr.get()->getMemBufferRef();
  auto libOrErr = llvm::object::Archive::create(buffer);
  exitIfError(libOrErr.takeError(), "cannot read " + path);

  // Decode all LLIR objects, dump the rest to text files.
  llvm::Error err = llvm::Error::success();
  std::vector<std::unique_ptr<Prog>> progs;
  for (auto &child : libOrErr.get()->children(err)) {
    auto nameOrErr = child.getName();
    exitIfError(nameOrErr.takeError(), "cannot read name " + path);
    llvm::outs() << nameOrErr.get() << "\n";
  }

  if (err) {
    llvm::WithColor::error(llvm::errs(), ToolName)
        << "cannot list archive '" << path << "': " << err << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  ToolName = (argc == 0 ? "llir-ar" : argv[0]);
  if (argc < 3) {
    llvm::errs() << "Usage: " << ToolName << "{dtqrc} archive-file file...";
    return EXIT_FAILURE;
  }

  llvm::StringRef archive(argv[2]);
  std::vector<llvm::StringRef> objs;
  for (int i = 3; i < argc; ++i) {
    objs.emplace_back(argv[i]);
  }

  bool do_index   = false;
  bool do_delete  = false;
  bool do_list    = false;
  bool do_quick   = false;
  bool do_replace = false;
  bool do_create  = false;
  bool do_update  = false;
  bool do_extract = false;
  bool do_verbose = false;
  for (const char *ch = argv[1]; *ch; ++ch) {
    switch (*ch) {
      case 'd': do_delete  = true; continue;
      case 't': do_list    = true; continue;
      case 'q': do_quick   = true; continue;
      case 'r': do_replace = true; continue;
      case 'c': do_create  = true; continue;
      case 's': do_index   = true; continue;
      case 'u': do_update  = true; continue;
      case 'x': do_extract = true; continue;
      case 'v': do_verbose = true; continue;
    }
    WithColor::error(llvm::errs(), ToolName) << "invalid command: " << *ch << "\n";
    return EXIT_FAILURE;
  }

  if (!(do_delete || do_list || do_quick || do_replace || do_extract)) {
    WithColor::error(llvm::errs(), ToolName) << "no action specified\n";
    return EXIT_FAILURE;
  }

  if ((int)do_delete + (int)do_list + (int)do_quick + (int)do_replace > 1) {
    WithColor::error(llvm::errs(), ToolName) << "multiple actions\n";
    return EXIT_FAILURE;
  }

  if (do_delete) {
    llvm_unreachable("not implemented");
  }

  if (do_quick || do_replace) {
    if (!do_create && !sys::fs::exists(archive)) {
      llvm::outs() << "creating " << archive << "\n";
    }
    return CreateOrUpdateArchive(argv[2], objs);
  }

  if (do_extract) {
    return ExtractArchive(argv[2], do_verbose);
  }

  if (do_list) {
    return ListArchive(argv[2]);
  }

  llvm_unreachable("not implemented");
}
