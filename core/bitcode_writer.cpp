// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/bitcode.h"

#include <llvm/Support/Endian.h>
#include <llvm/ADT/PostOrderIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/data.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/xtor.h"

namespace endian = llvm::support::endian;



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
void BitcodeWriter::Write(const Prog &prog)
{
  // Write the header.
  Emit<uint32_t>(kLLIRMagic);

  // Emit the program name.
  Emit(prog.getName());

  // Write all symbols and their names.
  {
    // Externs.
    Emit<uint32_t>(prog.ext_size());
    for (const Extern &ext : prog.externs()) {
      Emit(ext.getName());
      symbols_.emplace(&ext, symbols_.size());
    }

    // Atoms.
    Emit<uint32_t>(prog.data_size());
    for (const Data &data : prog.data()) {
      Emit(data.getName());
      Emit<uint32_t>(data.size());
      for (const Object &object : data) {
        Emit<uint32_t>(object.size());
        for (const Atom &atom : object) {
          Emit(atom.getName());
          symbols_.emplace(&atom, symbols_.size());
        }
      }
    }

    // Functions.
    Emit<uint32_t>(prog.size());
    for (const Func &func : prog.funcs()) {
      Emit(func.getName());
      symbols_.emplace(&func, symbols_.size());
      llvm::ReversePostOrderTraversal<const Func *> rpot(&func);
      Emit<uint32_t>(std::distance(rpot.begin(), rpot.end()));
      {
        for (const Block *block : rpot) {
          Emit(block->getName());
          Emit<uint8_t>(static_cast<uint8_t>(block->GetVisibility()));
          symbols_.emplace(block, symbols_.size());
        }
      }
    }
  }

  // Emit all data items.
  for (const Data &data : prog.data()) {
    for (const Object &object : data) {
      Emit<uint8_t>(object.IsThreadLocal());
      for (const Atom &atom : object) {
        Write(atom);
      }
    }
  }

  // Emit all functions.
  for (const Func &func : prog) {
    Write(func);
  }

  // Emit all extern aliases.
  for (const Extern &ext : prog.externs()) {
    Write(ext);
  }

  // Emit all ctors and dtors.
  Emit<uint32_t>(prog.xtor_size());
  for (const Xtor &xtor : prog.xtor()) {
    Write(xtor);
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Func &func)
{
  // Emit attributes.
  if (auto align = func.GetAlignment()) {
    Emit<uint32_t>(static_cast<uint32_t>(align->value()));
  } else {
    Emit<uint32_t>(0);
  }
  Emit<uint8_t>(static_cast<uint8_t>(func.GetVisibility()));
  Emit<uint8_t>(static_cast<uint8_t>(func.GetCallingConv()));
  Emit<uint8_t>(func.IsVarArg());
  Emit<uint8_t>(func.IsNoInline());

  // Emit CPU and feature strings.
  Emit(func.getCPU());
  Emit(func.getTuneCPU());
  Emit(func.getFeatures());

  // Emit stack objects.
  {
    llvm::ArrayRef<Func::StackObject> objects = func.objects();
    Emit<uint16_t>(objects.size());
    for (const Func::StackObject &obj : objects) {
      Emit<uint16_t>(obj.Index);
      Emit<uint32_t>(obj.Size);
      Emit<uint8_t>(obj.Alignment.value());
    }
  }

  // Emit parameters.
  {
    llvm::ArrayRef<FlaggedType> params = func.params();
    Emit<uint16_t>(params.size());
    for (FlaggedType type : params) {
      Write(type);
    }
  }

  // Emit personality.
  if (auto pers = func.GetPersonality()) {
    Write(*pers);
  } else {
    Emit<uint32_t>(0);
  }

  // Emit BBs and instructions.
  {
    std::unordered_map<ConstRef<Inst>, unsigned> map;
    llvm::ReversePostOrderTraversal<const Func *> rpot(&func);
    for (const Block *block : rpot) {
      for (const Inst &inst : *block) {
        for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
          map.emplace(ConstRef<Inst>(&inst, i), map.size() + 1);
        }
      }
    }

    for (const Block *block : rpot) {
      Emit<uint32_t>(block->size());
      for (const Inst &inst : *block) {
        Write(inst, map);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Atom &atom)
{
  if (auto align = atom.GetAlignment()) {
    Emit<uint32_t>(static_cast<uint32_t>(align->value()));
  } else {
    Emit<uint32_t>(0);
  }
  Emit<uint8_t>(static_cast<uint8_t>(atom.GetVisibility()));
  Emit<uint32_t>(atom.size());
  for (const Item &item : atom) {
    Item::Kind kind = item.GetKind();
    Emit<uint8_t>(static_cast<uint8_t>(kind));
    switch (kind) {
      case Item::Kind::INT8: {
        Emit<int8_t>(item.GetInt8());
        continue;
      }
      case Item::Kind::INT16: {
        Emit<int16_t>(item.GetInt16());
        continue;
      }
      case Item::Kind::INT32: {
        Emit<int32_t>(item.GetInt32());
        continue;
      }
      case Item::Kind::INT64: {
        Emit<int64_t>(item.GetInt64());
        continue;
      }
      case Item::Kind::FLOAT64: {
        Emit<double>(item.GetFloat64());
        continue;
      }
      case Item::Kind::EXPR32:
      case Item::Kind::EXPR64: {
        Write(*item.GetExpr());
        continue;
      }
      case Item::Kind::SPACE: {
        Emit<uint32_t>(item.GetSpace());
        continue;
      }
      case Item::Kind::STRING: {
        Emit(item.getString());
        continue;
      }
    }
    llvm_unreachable("invalid item kind");
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Extern &ext)
{
  Emit<uint8_t>(static_cast<uint8_t>(ext.GetVisibility()));
  if (auto v = ext.GetValue()) {
    Emit<uint8_t>(1);
    Write(v, {});
  } else {
    Emit<uint8_t>(0);
  }
  if (auto symbol = ext.GetSection()) {
    Emit<uint8_t>(1);
    Emit(std::string(*symbol));
  } else {
    Emit<uint8_t>(0);
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(
    const Inst &i,
    const std::unordered_map<ConstRef<Inst>, unsigned> &map)
{
  // Emit the annotations.
  Emit<uint8_t>(i.annot_size());
  for (const auto &annot : i.annots()) {
    Write(annot);
  }
  // Emit the instruction kind.
  Emit<uint8_t>(static_cast<uint8_t>(i.GetKind()));
  switch (i.GetKind()) {
    case Inst::Kind::PHI: {
      auto &phi = static_cast<const PhiInst &>(i);
      // Write the type.
      Write(phi.GetType());
      // Write argument pairs.
      unsigned n = phi.GetNumIncoming();
      Emit<uint16_t>(n);
      for (unsigned i = 0; i < n; ++i) {
        Write(phi.GetBlock(i), map);
        auto it = map.find(cast<Inst>(phi.GetValue(i)));
        assert(it != map.end() && "missing instruction");
        Emit<uint32_t>(it->second);
      }
      return;
    }
    #define GET_BITCODE_WRITER
    #include "instructions.def"
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Expr &expr)
{
  Emit<uint8_t>(static_cast<uint8_t>(expr.GetKind()));
  switch (expr.GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto &offsetExpr = static_cast<const SymbolOffsetExpr &>(expr);
      if (auto *symbol = offsetExpr.GetSymbol()) {
        Write(*symbol);
      } else {
        Emit<uint32_t>(0);
      }
      Emit<int64_t>(offsetExpr.GetOffset());
      return;
    }
  }
  llvm_unreachable("invalid expression kind");
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Global &global)
{
  auto it = symbols_.find(&global);
  assert(it != symbols_.end() && "missing symbol");
  Emit<uint32_t>(it->second + 1);
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Annot &annot)
{
  Emit<uint8_t>(static_cast<uint8_t>(annot.GetKind()));
  switch (annot.GetKind()) {
    case Annot::Kind::CAML_FRAME: {
      auto &frame = static_cast<const CamlFrame &>(annot);
      Emit<uint8_t>(frame.alloc_size());
      for (const auto &alloc : frame.allocs()) {
        Emit<size_t>(alloc);
      }
      Emit<uint8_t>(frame.debug_info_size());
      for (const auto &debug_info : frame.debug_infos()) {
        Emit<uint8_t>(debug_info.size());
        for (const auto &debug : debug_info) {
          Emit<int64_t>(debug.Location);
          Emit(debug.File);
          Emit(debug.Definition);
        }
      }
      return;
    }
    case Annot::Kind::PROBABILITY: {
      auto &p = static_cast<const Probability &>(annot);
      Emit<uint32_t>(p.GetNumerator());
      Emit<uint32_t>(p.GetDenumerator());
      return;
    }
  }
  llvm_unreachable("invalid annotation kind");
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Xtor &xtor)
{
  Emit<uint8_t>(static_cast<uint8_t>(xtor.GetKind()));
  Emit<int32_t>(xtor.GetPriority());
  auto it = symbols_.find(xtor.GetFunc());
  assert(it != symbols_.end() && "missing symbol");
  Emit<uint32_t>(it->second);
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(Type type)
{
  Emit<uint8_t>(static_cast<uint8_t>(type));
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const TypeFlag &flag)
{
  Emit<uint8_t>(static_cast<uint8_t>(flag.GetKind()));
  switch (flag.GetKind()) {
    case TypeFlag::Kind::NONE:
    case TypeFlag::Kind::SEXT:
    case TypeFlag::Kind::ZEXT: {
      return;
    }
    case TypeFlag::Kind::BYVAL: {
      Emit<uint16_t>(flag.GetByValSize());
      Emit<uint16_t>(flag.GetByValAlign().value());
      return;
    }
  }
  llvm_unreachable("invalid flag kind");
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const FlaggedType &type)
{
  Write(type.GetType());
  Write(type.GetFlag());
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(
    ConstRef<Value> value,
    const std::unordered_map<ConstRef<Inst>, unsigned> &map)
{
  auto valueKind = value->GetKind();
  Emit<uint8_t>(static_cast<uint8_t>(valueKind));
  switch (valueKind) {
    case Value::Kind::INST: {
      auto it = map.find(cast<Inst>(value));
      assert(it != map.end() && "missing instruction");
      Emit<uint32_t>(it->second);
      return;
    }
    case Value::Kind::GLOBAL: {
      auto it = symbols_.find(&*cast<Global>(value));
      assert(it != symbols_.end() && "missing symbol");
      Emit<uint32_t>(it->second);
      return;
    }
    case Value::Kind::EXPR: {
      Write(*cast<Expr>(value));
      return;
    }
    case Value::Kind::CONST: {
      Write(cast<Constant>(value));
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(
    ConstRef<Inst> value,
    const std::unordered_map<ConstRef<Inst>, unsigned> &map)
{
  if (value) {
    auto it = map.find(value);
    assert(it != map.end() && it->second <= map.size() && "missing instruction");
    Emit<uint32_t>(it->second);
  } else {
    Emit<uint32_t>(0);
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(
    const Block *value,
    const std::unordered_map<ConstRef<Inst>, unsigned> &map)
{
  auto it = symbols_.find(value);
  assert(it != symbols_.end() && "missing symbol");
  Emit<uint32_t>(it->second);
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(ConstRef<Constant> c)
{
  auto constKind = c->GetKind();
  Emit<uint8_t>(static_cast<uint8_t>(constKind));
  switch (constKind) {
    case Constant::Kind::INT: {
      auto v = ::cast<ConstantInt>(c)->GetInt();
      Emit<int64_t>(v);
      return;
    }
    case Constant::Kind::FLOAT: {
      auto v = ::cast<ConstantFloat>(c)->GetDouble();
      Emit<double>(v);
      return;
    }
  }
  llvm_unreachable("invalid constant kind");
}
