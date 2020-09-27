// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/WithColor.h>

#include "core/bitcode.h"

namespace sys = llvm::sys;
using WithColor = llvm::WithColor;



// -----------------------------------------------------------------------------
template<typename T>
void Write(llvm::raw_ostream &os, T t)
{
  char buffer[sizeof(T)];
  llvm::support::endian::write(buffer, t, llvm::support::little);
  os.write(buffer, sizeof(buffer));
}

// -----------------------------------------------------------------------------
template<typename T>
void Fixup(llvm::raw_pwrite_stream &os, T t, uint64_t offset)
{
  char buffer[sizeof(T)];
  llvm::support::endian::write(buffer, t, llvm::support::little);
  os.pwrite(buffer, sizeof(buffer), offset);
}

// -----------------------------------------------------------------------------
int CreateArchive(
    const char *argv0,
    llvm::StringRef archive,
    llvm::ArrayRef<char *> objs)
{
  // Open the output stream.
  std::error_code err;
  auto output = std::make_unique<llvm::ToolOutputFile>(
      archive,
      err,
      sys::fs::F_None
  );
  if (err) {
    WithColor::error(llvm::errs(), argv0)
        << "cannot open for writing: " << err.message() << "\n";
    return EXIT_FAILURE;
  }

  llvm::raw_pwrite_stream &s = output->os();
  Write<uint64_t>(s, 0x52414C4C);
  Write<uint64_t>(s, objs.size());
  for (unsigned i = 0, n = objs.size(); i < n; ++i) {
    Write<size_t>(s, 0);
    Write<uint64_t>(s, 0);
  }

  uint64_t meta = sizeof(uint64_t) + sizeof(uint64_t);
  uint64_t offset = meta + (sizeof(size_t) + sizeof(uint64_t)) * objs.size();
  for (unsigned i = 0, n = objs.size(); i < n; ++i) {
    // Read the object file into memory.
    auto FileOrErr = llvm::MemoryBuffer::getFile(objs[i]);
    if (auto err = FileOrErr.getError()) {
      WithColor::error(llvm::errs(), argv0)
          << "cannot open '" << objs[i] << "' for reading: "
          << err.message() << "\n";
      return false;
    }

    auto buffer = FileOrErr.get()->getMemBufferRef();
    if (!IsLLIRObject(buffer.getBuffer())) {
      WithColor::error(llvm::errs(), argv0)
          << "not an LLIR object: " << objs[i] << "\n";
      return false;
    }
    size_t size = buffer.getBufferSize();

    // Adjust the size and offset fields.
    Fixup<size_t>(s, size, meta);
    Fixup<uint64_t>(s, offset, meta + sizeof(uint64_t));
    size_t step = sizeof(size_t) + sizeof(uint64_t);
    offset += size;
    meta += step;

    // Write the contents.
    s.write(buffer.getBufferStart(), size);
  }

  output->keep();
  return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  if (argc < 3) {
    llvm::errs()
      << "Usage: " << (argc == 0 ? "llir-ar" : argv[0])
      << "{dtqrc} archive-file file...";
    return EXIT_FAILURE;
  }

  llvm::StringRef archive(argv[2]);
  llvm::ArrayRef objs(argv + 3, argv + argc);

  bool do_index   = false;
  bool do_delete  = false;
  bool do_list    = false;
  bool do_quick   = false;
  bool do_replace = false;
  bool do_create  = false;
  bool do_update  = false;
  for (const char *ch = argv[1]; *ch; ++ch) {
    switch (*ch) {
      case 'd': do_delete  = true; continue;
      case 't': do_list    = true; continue;
      case 'q': do_quick   = true; continue;
      case 'r': do_replace = true; continue;
      case 'c': do_create  = true; continue;
      case 's': do_index   = true; continue;
      case 'u': do_update  = true; continue;
    }
    WithColor::error(llvm::errs(), argv[0]) << "invalid command: " << *ch << "\n";
    return EXIT_FAILURE;
  }

  if (!(do_delete || do_list || do_quick || do_replace)) {
    WithColor::error(llvm::errs(), argv[0]) << "no action specified\n";
    return EXIT_FAILURE;
  }

  if ((int)do_delete + (int)do_list + (int)do_quick + (int)do_replace > 1) {
    WithColor::error(llvm::errs(), argv[0]) << "multiple actions\n";
    return EXIT_FAILURE;
  }

  if (do_delete) {
    llvm_unreachable("not implemented");
  }

  if (do_quick || do_replace) {
    if (!do_create && !sys::fs::exists(archive)) {
      llvm::outs() << "creating " << archive << "\n";
    }

    if (!sys::fs::exists(archive)) {
      return CreateArchive(argv[0], argv[2], objs);
    }
    llvm_unreachable("not implemented");
  }

  llvm_unreachable("not implemented");
}
