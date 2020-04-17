// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Endian.h>

#include "core/bitcode.h"
#include "core/block.h"
#include "core/data.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"


// -----------------------------------------------------------------------------
Prog *BitcodeReader::Read()
{
  abort();
}

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
  Emit<uint32_t>(kBitcodeMagic);

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
void BitcodeWriter::Write(const Func &func)
{
  Emit<uint8_t>(static_cast<uint8_t>(func.GetAlignment()));
  Emit<uint8_t>(static_cast<uint8_t>(func.GetVisibility()));
  Emit<uint8_t>(func.IsVarArg());

  llvm::ArrayRef<Func::StackObject> objects = func.objects();
  Emit<uint32_t>(objects.size());
  for (const Func::StackObject &obj : objects) {
    llvm_unreachable("not implemented");
  }

  llvm::ArrayRef<Type> params = func.params();
  Emit<uint32_t>(params.size());
  for (Type type : params) {
    Emit<uint8_t>(static_cast<uint8_t>(type));
  }

  std::unordered_map<const Inst *, unsigned> map;
  for (const Block &block : func) {
    for (const Inst &inst : block) {
      map.emplace(&inst, map.size());
    }
  }

  Emit<uint32_t>(func.size());
  for (const Block &block : func) {
    Emit<uint32_t>(block.size());
    for (const Inst &inst : block) {
      Write(inst, map);
    }
  }
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
          auto *expr = item->GetExpr();
          switch (expr->GetKind()) {
            case Expr::Kind::SYMBOL_OFFSET: {
              auto *offsetExpr = static_cast<SymbolOffsetExpr *>(expr);
              if (auto *symbol = offsetExpr->GetSymbol()) {
                auto it = symbols_.find(symbol);
                assert(it != symbols_.end() && "missing symbol");
                Emit<uint32_t>(it->second);
              } else {
                Emit<uint64_t>(0);
              }
              Emit<int64_t>(offsetExpr->GetOffset());
              break;
            }
          }
          break;
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

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(
    const Inst &inst,
    const std::unordered_map<const Inst *, unsigned> &map)
{
  Emit<uint8_t>(static_cast<uint8_t>(inst.GetKind()));

  uint32_t annots = 0;
  for (const auto &annot : inst.annots()) {
    annots |= (1 << static_cast<uint32_t>(annot));
  }
  Emit<uint32_t>(annots);

  if (inst.GetNumRets() > 0) {
    Emit<uint8_t>(static_cast<uint8_t>(inst.GetType(0)) + 1);
  } else {
    Emit<uint8_t>(0);
  }

  if (auto size = inst.GetSize()) {
    Emit<uint8_t>(*size + 1);
  } else {
    Emit<uint8_t>(0);
  }

  switch (inst.GetKind()) {
    case Inst::Kind::CALL: {
      auto cc = static_cast<const CallInst *>(&inst)->GetCallingConv();
      Emit<uint8_t>(static_cast<uint8_t>(cc));
      break;
    }
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE:
    case Inst::Kind::TINVOKE: {
      auto *callInst = static_cast<const CallSite<TerminatorInst> *>(&inst);
      auto cc = callInst->GetCallingConv();
      Emit<uint8_t>(static_cast<uint8_t>(cc));
      break;
    }
    case Inst::Kind::CMP: {
      auto cc = static_cast<const CmpInst *>(&inst)->GetCC();
      Emit<uint8_t>(static_cast<uint8_t>(cc));
      break;
    }
    default: {
      break;
    }
  }

  for (const Value *value : inst.operand_values()) {
    auto valueKind = value->GetKind();
    Emit<uint8_t>(static_cast<uint8_t>(valueKind));
    switch (valueKind) {
      case Value::Kind::INST: {
        auto it = map.find(static_cast<const Inst *>(value));
        assert(it != map.end() && "missing instruction");
        Emit<uint32_t>(it->second);
        break;
      }
      case Value::Kind::GLOBAL: {
        auto it = symbols_.find(static_cast<const Global *>(value));
        assert(it != symbols_.end() && "missing symbol");
        Emit<uint32_t>(it->second);
        break;
      }
      case Value::Kind::EXPR: {
        llvm_unreachable("not implemented");
      }
      case Value::Kind::CONST: {
        auto constKind = static_cast<const Constant *>(value)->GetKind();
        switch (constKind) {
          case Constant::Kind::INT: {
            Emit<int64_t>(static_cast<const ConstantInt *>(value)->GetInt());
            break;
          }
          case Constant::Kind::FLOAT: {
            Emit<double>(static_cast<const ConstantFloat *>(value)->GetDouble());
            break;
          }
          case Constant::Kind::REG: {
            llvm_unreachable("not implemented");
          }
        }
      }
    }
  }
}
