// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Endian.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include "core/bitcode.h"
#include "core/parser.h"
#include "core/prog.h"
#include "core/util.h"

namespace endian = llvm::support::endian;



// -----------------------------------------------------------------------------
template<typename T, const uint64_t Magic>
T CheckMagic(llvm::StringRef buffer, uint64_t offset)
{
  namespace endian = llvm::support::endian;
  if (offset + sizeof(T) > buffer.size()) {
    return false;
  }
  auto *data = buffer.data() + offset;
  return endian::read<T, llvm::support::little, 1>(data) == Magic;
}

// -----------------------------------------------------------------------------
bool IsLLIRObject(llvm::StringRef buffer)
{
  return CheckMagic<uint32_t, kLLIRMagic>(buffer, 0);
}

// -----------------------------------------------------------------------------
bool IsLLARArchive(llvm::StringRef buffer)
{
  return CheckMagic<uint32_t, kLLARMagic>(buffer, 0);
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> Parse(llvm::StringRef buffer, std::string_view name)
{
  if (ReadData<uint32_t>(buffer, 0) != kLLIRMagic) {
    return Parser(buffer, name).Parse();
  }
  return BitcodeReader(buffer).Read();
}

// -----------------------------------------------------------------------------
std::string Abspath(const std::string &path)
{
  llvm::SmallString<256> result{llvm::StringRef(path)};
  llvm::sys::fs::make_absolute(result);
  llvm::sys::path::remove_dots(result);
  return llvm::StringRef(result).str();
}

// -----------------------------------------------------------------------------
std::string ParseToolName(const std::string &argv0, const char *tool)
{
  auto file = llvm::sys::path::filename(argv0).str();
  auto dash = file.find_last_of('-');
  if (dash == std::string::npos) {
    return "";
  }
  std::string triple = file.substr(0, dash);
  if (triple == "llir") {
    return "";
  }
  return triple;
}
