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
std::unique_ptr<Prog> Parse(llvm::StringRef buffer, std::string_view name)
{
  if (ReadData<uint32_t>(buffer, 0) != kLLIRMagic) {
    return Parser(buffer, name).Parse();
  }
  return BitcodeReader(buffer).Read();
}

// -----------------------------------------------------------------------------
std::string abspath(const std::string &path)
{
  llvm::SmallString<256> result{llvm::StringRef(path)};
  llvm::sys::fs::make_absolute(result);
  llvm::sys::path::remove_dots(result);
  return llvm::StringRef(result).str();
}
