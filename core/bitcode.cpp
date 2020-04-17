// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Endian.h>

#include "core/bitcode.h"
#include "core/data.h"
#include "core/func.h"
#include "core/block.h"
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
  // Write the header.
  Emit<uint32_t>(0x52494C4C);

  // Write all symbols and their names.
  {
    // Externs.
    Emit<uint32_t>(prog->ext_size());
    for (const Extern &ext : prog->externs()) {
      Emit(ext.getName());
      symbols_.emplace(&ext, symbols_.size());
    }

    // Atoms.
    Emit<uint32_t>(prog->data_size());
    for (const Data &data : prog->data()) {
      for (const Atom &atom : data) {
        Emit(atom.getName());
        symbols_.emplace(&atom, symbols_.size());
      }
    }

    // Functions.
    Emit<uint32_t>(prog->size());
    for (const Func &func : prog->funcs()) {
      Emit(func.getName());
      symbols_.emplace(&func, symbols_.size());
      Emit<uint32_t>(func.size());
      for (const Block &block : func) {
        Emit(block.getName());
        symbols_.emplace(&block, symbols_.size());
      }
    }
  }

  // Emit all data items.
  for (const Data &data : prog->data()) {
    Write(data);
  }

  // Emit all functions.
  for (const Func &func : *prog) {
    Write(func);
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Func &prog)
{
  llvm_unreachable("Func");
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Data &data)
{
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
        case Item::Kind::EXPR: {
          /*
          Global *global = item->GetSymbol();
          auto it = symbols_.find(global);
          if (it == symbols_.end()) {
            llvm::report_fatal_error(("missing symbol: "s + global->getName()));
          }
          Emit<uint32_t>(it->second);
          llvm_unreachable("SYMBOL");
          continue;
          */
          llvm_unreachable("WTF");
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
}
