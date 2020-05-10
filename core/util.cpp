// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Endian.h>

#include "core/bitcode.h"
#include "core/parser.h"
#include "core/prog.h"
#include "core/util.h"

namespace endian = llvm::support::endian;



// -----------------------------------------------------------------------------
static bool CheckBitcodeMagic(llvm::StringRef buffer)
{
  if (buffer.size() < sizeof(kBitcodeMagic)) {
    return false;
  }

  auto *buf = buffer.data();
  auto magic = endian::read<uint32_t, llvm::support::little, 1>(buf);
  return magic == kBitcodeMagic;
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> Parse(llvm::StringRef buffer)
{
  if (!CheckBitcodeMagic(buffer)) {
    return Parser(buffer).Parse();
  }
  return BitcodeReader(buffer).Read();
}
