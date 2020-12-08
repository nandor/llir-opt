// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/bitcode.h"
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
template<typename T> T BitcodeReader::ReadData()
{
  if (offset_ + sizeof(T) > buf_.size()) {
    llvm::report_fatal_error("invalid bitcode file");
  }

  auto *data = buf_.data() + offset_;
  offset_ += sizeof(T);
  return endian::read<T, llvm::support::little, 1>(data);
}

// -----------------------------------------------------------------------------
template<typename T> std::optional<T> BitcodeReader::ReadOptional()
{
  T v = ReadData<T>();
  return v ? std::optional(v - 1) : std::nullopt;
}

// -----------------------------------------------------------------------------
std::string BitcodeReader::ReadString()
{
  uint32_t size = ReadData<uint32_t>();
  const char *ptr = buf_.data();
  if (offset_ + size > buf_.size()) {
    llvm::report_fatal_error("invalid bitcode file: string too long");
  }
  std::string s(ptr + offset_, ptr + offset_ + size);
  offset_ += size;
  return s;
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> BitcodeReader::Read()
{
  // Check the magic.
  if (ReadData<uint32_t>() != kLLIRMagic) {
    llvm::report_fatal_error("invalid bitcode magic");
  }

  // Read all symbols and their names.
  auto prog = std::make_unique<Prog>(ReadString());
  {
    // Externs.
    for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
      Extern *ext = new Extern(ReadString());
      prog->AddExtern(ext);
      globals_.push_back(ext);
    }

    // Data segments and atoms.
    for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
      Data *data = prog->GetOrCreateData(ReadString());
      for (unsigned j = 0, m = ReadData<uint32_t>(); j < m; ++j) {
        Object *object = new Object();
        data->AddObject(object);
        for (unsigned k = 0, p = ReadData<uint32_t>(); k < p; ++k) {
          Atom *atom = new Atom(ReadString());
          object->AddAtom(atom);
          globals_.push_back(atom);
        }
      }
    }

    // Functions.
    for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
      Func *func = new Func(ReadString());
      globals_.push_back(func);
      for (unsigned j = 0, m = ReadData<uint32_t>(); j < m; ++j) {
        auto name = ReadString();
        auto vis = static_cast<Visibility>(ReadData<uint8_t>());
        Block *block = new Block(name, vis);
        globals_.push_back(block);
        func->AddBlock(block);
      }
      prog->AddFunc(func);
    }
  }

  // Read all data items.
  for (Data &data : prog->data()) {
    for (Object &object : data) {
      for (Atom &atom : object) {
        Read(atom);
      }
    }
  }

  // Read all functions.
  for (Func &func : *prog) {
    Read(func);
  }

  // Read externs.
  for (Extern &ext : prog->externs()) {
    Read(ext);
  }

  // Read constructor/destructors.
  for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
    prog->AddXtor(ReadXtor());
  }

  return std::move(prog);
}

// -----------------------------------------------------------------------------
void BitcodeReader::Read(Func &func)
{
  if (auto align = ReadData<uint32_t>()) {
    func.SetAlignment(llvm::Align(align));
  }
  func.SetVisibility(static_cast<Visibility>(ReadData<uint8_t>()));
  func.SetCallingConv(static_cast<CallingConv>(ReadData<uint8_t>()));
  func.SetVarArg(ReadData<uint8_t>());
  func.SetNoInline(ReadData<uint8_t>());
  func.SetFeatures(ReadString());

  // Read stack objects.
  {
    for (unsigned i = 0, n = ReadData<uint16_t>(); i < n; ++i) {
      auto Index = ReadData<uint16_t>();
      auto Size = ReadData<uint32_t>();
      auto Alignment = ReadData<uint8_t>();
      func.AddStackObject(Index, Size, llvm::Align(Alignment));
    }
  }

  // Read parameters.
  {
    std::vector<FlaggedType> parameters;
    for (unsigned i = 0, n = ReadData<uint16_t>(); i < n; ++i) {
      parameters.push_back(ReadFlaggedType());
    }
    func.SetParameters(parameters);
  }

  // Read blocks.
  {
    std::vector<Ref<Inst>> map;
    std::vector<std::tuple<PhiInst *, Block *, unsigned>> fixups;
    for (Block &block : func) {
      for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
        Inst *inst = ReadInst(map, fixups);
        block.AddInst(inst);
        for (unsigned j = 0, n = inst->GetNumRets(); j < n; ++j) {
          map.emplace_back(inst, j);
        }
      }
    }
    for (auto &[phi, block, idx] : fixups) {
      if (idx >= map.size()) {
        llvm::report_fatal_error("missing instruction");
      }
      phi->Add(block, map[idx]);
    }
  }
}

// -----------------------------------------------------------------------------
void BitcodeReader::Read(Atom &atom)
{
  if (auto align = ReadData<uint32_t>()) {
    atom.SetAlignment(llvm::Align(align));
  }
  atom.SetVisibility(static_cast<Visibility>(ReadData<uint8_t>()));
  for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
    switch (static_cast<Item::Kind>(ReadData<uint8_t>())) {
      case Item::Kind::INT8: {
        atom.AddItem(new Item(ReadData<int8_t>()));
        continue;
      }
      case Item::Kind::INT16: {
        atom.AddItem(new Item(ReadData<int16_t>()));
        continue;
      }
      case Item::Kind::INT32: {
        atom.AddItem(new Item(ReadData<int32_t>()));
        continue;
      }
      case Item::Kind::INT64: {
        atom.AddItem(new Item(ReadData<int64_t>()));
        continue;
      }
      case Item::Kind::FLOAT64: {
        atom.AddItem(new Item(ReadData<double>()));
        continue;
      }
      case Item::Kind::EXPR: {
        atom.AddItem(new Item(ReadExpr()));
        continue;
      }
      case Item::Kind::ALIGN: {
        atom.AddItem(new Item(Item::Align{ ReadData<uint8_t>() }));
        continue;
      }
      case Item::Kind::SPACE: {
        atom.AddItem(new Item(Item::Space{ ReadData<uint32_t>() }));
        continue;
      }
      case Item::Kind::STRING: {
        atom.AddItem(new Item(ReadString()));
        continue;
      }
    }
    llvm_unreachable("invalid item kind");
  }
}

// -----------------------------------------------------------------------------
void BitcodeReader::Read(Extern &ext)
{
  ext.SetVisibility(static_cast<Visibility>(ReadData<uint8_t>()));
  if (auto id = ReadData<uint32_t>()) {
    ext.SetAlias(globals_[id]);
  }
  if (ReadData<uint8_t>()) {
    ext.SetSection(ReadString());
  }
}

// -----------------------------------------------------------------------------
Inst *BitcodeReader::ReadInst(
      const std::vector<Ref<Inst>> &map,
      std::vector<std::tuple<PhiInst *, Block *, unsigned>> &fixups)
{
  // Parse annotations.
  AnnotSet annots;
  for (unsigned i = 0, n = ReadData<uint8_t>(); i < n; ++i) {
    ReadAnnot(annots);
  }

  // Decode the rest.
  switch (static_cast<Inst::Kind>(ReadData<uint8_t>())) {
    case Inst::Kind::PHI: {
      // Parse the type.
      Type type = static_cast<Type>(ReadData<uint8_t>());
      // Record fixups.
      PhiInst *phi = new PhiInst(type, std::move(annots));
      for (unsigned i = 0, n = ReadData<uint16_t>(); i < n; ++i) {
        Ref<Block> block = ReadBlock(map);
        uint32_t index = ReadData<uint32_t>();
        if (index >= map.size()) {
          fixups.emplace_back(phi, block.Get(), index);
        } else {
          phi->Add(block.Get(), map[index]);
        }
      }
      return phi;
    }
    #define GET_BITCODE_READER
    #include "instructions.def"
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
Expr *BitcodeReader::ReadExpr()
{
  switch (static_cast<Expr::Kind>(ReadData<uint8_t>())) {
    case Expr::Kind::SYMBOL_OFFSET: {
      Global *global;
      if (auto index = ReadData<uint32_t>()) {
        if (index - 1 >= globals_.size()) {
          llvm::report_fatal_error("invalid global index");
        }
        global = globals_[index - 1];
      } else {
        global = nullptr;
      }
      auto offset = ReadData<int64_t>();
      return new SymbolOffsetExpr(global, offset);
    }
  }
  llvm_unreachable("invalid expression kind");
}

// -----------------------------------------------------------------------------
Ref<Value> BitcodeReader::ReadValue(const std::vector<Ref<Inst>> &map)
{
  switch (static_cast<Value::Kind>(ReadData<uint8_t>())) {
    case Value::Kind::INST: {
      uint32_t index = ReadData<uint32_t>();
      if (index >= map.size()) {
        llvm::report_fatal_error("invalid instruction index");
      }
      return map[index];
    }
    case Value::Kind::GLOBAL: {
      uint32_t index = ReadData<uint32_t>();
      if (index >= globals_.size()) {
        llvm::report_fatal_error("invalid global index");
      }
      return globals_[index];
    }
    case Value::Kind::EXPR: {
      return ReadExpr();
    }
    case Value::Kind::CONST: {
      switch (static_cast<Constant::Kind>(ReadData<uint8_t>())) {
        case Constant::Kind::INT: {
          auto v = ReadData<int64_t>();
          return new ConstantInt(v);
        }
        case Constant::Kind::FLOAT: {
          auto v = ReadData<double>();
          return new ConstantFloat(v);
        }
        case Constant::Kind::REG: {
          auto v = static_cast<ConstantReg::Kind>(ReadData<uint8_t>());
          return new ConstantReg(v);
        }
      }
      llvm_unreachable("invalid constant kind");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
Block * BitcodeReader::ReadBlock(const std::vector<Ref<Inst>> &map)
{
  uint32_t index = ReadData<uint32_t>();
  if (index >= globals_.size()) {
    llvm::report_fatal_error("invalid global index");
  }
  return ::cast<Block>(globals_[index]);
}

// -----------------------------------------------------------------------------
Ref<Inst> BitcodeReader::ReadInst(const std::vector<Ref<Inst>> &map)
{
  uint32_t index = ReadData<uint32_t>();
  if (index >= map.size()) {
    llvm::report_fatal_error("invalid instruction index");
  }
  return map[index];
}

// -----------------------------------------------------------------------------
void BitcodeReader::ReadAnnot(AnnotSet &annots)
{
  switch (static_cast<Annot::Kind>(ReadData<uint8_t>())) {
    case Annot::Kind::CAML_FRAME: {
      std::vector<size_t> allocs;
      for (uint8_t i = 0, n = ReadData<uint8_t>(); i < n; ++i) {
        allocs.push_back(ReadData<size_t>());
      }
      std::vector<CamlFrame::DebugInfos> debug_infos;
      for (uint8_t i = 0, n = ReadData<uint8_t>(); i < n; ++i) {
        CamlFrame::DebugInfos debug_info;
        for (uint8_t j = 0, m = ReadData<uint8_t>(); j < m; ++j) {
          CamlFrame::DebugInfo debug;
          debug.Location = ReadData<int64_t>();
          debug.File = ReadString();
          debug.Definition = ReadString();
          debug_info.push_back(std::move(debug));
        }
        debug_infos.push_back(std::move(debug_info));
      }
      annots.Set<CamlFrame>(std::move(allocs), std::move(debug_infos));
      return;
    }
    case Annot::Kind::PROBABILITY: {
      uint32_t n = ReadData<uint32_t>();
      uint32_t d = ReadData<uint32_t>();
      annots.Set<Probability>(n, d);
      return;
    }
  }
  llvm_unreachable("invalid annotation kind");
}

// -----------------------------------------------------------------------------
Xtor *BitcodeReader::ReadXtor()
{
  Xtor::Kind kind = static_cast<Xtor::Kind>(ReadData<uint8_t>());
  int priority = ReadData<int32_t>();
  uint32_t index = ReadData<uint32_t>();
  if (index >= globals_.size()) {
    llvm::report_fatal_error("invalid global index");
  }
  return new Xtor(priority, globals_[index], kind);
}

// -----------------------------------------------------------------------------
Type BitcodeReader::ReadType()
{
  return static_cast<Type>(ReadData<uint8_t>());
}

// -----------------------------------------------------------------------------
FlaggedType BitcodeReader::ReadFlaggedType()
{
  Type type = ReadType();
  TypeFlag::Kind kind = static_cast<TypeFlag::Kind>(ReadData<uint8_t>());
  switch (kind) {
    case TypeFlag::Kind::NONE:
      return { type, TypeFlag::getNone() };
    case TypeFlag::Kind::SEXT:
      return { type, TypeFlag::getSExt() };
    case TypeFlag::Kind::ZEXT:
      return { type, TypeFlag::getZExt() };
    case TypeFlag::Kind::BYVAL: {
      unsigned size = ReadData<uint16_t>();
      llvm::Align align(ReadData<uint16_t>());
      return { type, TypeFlag::getByVal(size, align) };
    }
  }
  llvm_unreachable("invalid flag kind");
}
