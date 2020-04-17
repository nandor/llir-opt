// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Endian.h>

#include "core/bitcode.h"
#include "core/data.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/extern.h"


// -----------------------------------------------------------------------------
void BitcodeWriter::Emit(llvm::StringRef str)
{
  Emit<uint32_t>(str.size());
  os_.write(str.data(), str.size());
}

// -----------------------------------------------------------------------------
template<typename T>
void BitcodeWriter::Emit(T t)
{
  char buffer[sizeof(T)];
  llvm::support::endian::write(buffer, t, llvm::support::little);
  os_.write(buffer, sizeof(buffer));
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Prog *prog)
{
  /*
  // Write the header.
  Emit<uint32_t>(0x52494C4C);
  Emit<uint32_t>(prog->size());
  Emit<uint32_t>(prog->data_size());
  Emit<uint32_t>(prog->ext_size());

  // Write all externs.
  for (const Extern &ext : prog->externs()) {
    Write(ext);
  }

  // Write all data.
  for (const Data &data : prog->data()) {
    Write(data);
  }

  // Write all functions.
  for (const Func &func : prog->funcs()) {
    Write(func);
  }
  */
  abort();
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Func &prog)
{
  llvm_unreachable("Func");
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Data &data)
{
  /*
  Emit<uint32_t>(data.size());
  Emit(data.getName());
  for (const Atom &atom : data) {
    Emit(atom.getName());
    Emit<uint8_t>(atom.GetAlignment());
    for (const Item *item : atom) {
      Item::Kind kind = item->GetKind();
      Emit<uint8_t>(static_cast<uint8_t>(kind));
      switch (kind) {
        case Item::Kind::INT8: {
          Emit<uint8_t>(item->GetInt8());
          continue;
        }
        case Item::Kind::INT16: {
          Emit<uint16_t>(item->GetInt16());
          continue;
        }
        case Item::Kind::INT32: {
          Emit<uint32_t>(item->GetInt32());
          continue;
        }
        case Item::Kind::INT64: {
          Emit<uint64_t>(item->GetInt64());
          continue;
        }
        case Item::Kind::FLOAT64: {
          Emit<double>(item->GetFloat64());
          continue;
        }
        case Item::Kind::SYMBOL: {
          llvm_unreachable("SYMBOL");
          continue;
        }
        case Item::Kind::ALIGN: {
          Emit<uint8_t>(item->GetAlign());
          continue;
        }
        case Item::Kind::SPACE: {
          Emit<uint32_t>(item->GetSpace());
          continue;
        }
        case Item::Kind::STRING: {
          Emit(item->GetString());
          continue;
        }
        case Item::Kind::END: {
          continue;
        }
      }
      llvm_unreachable("invalid item kind");
    }
  }
  */
  abort();
}
