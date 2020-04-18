// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Endian.h>

#include "core/bitcode.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/data.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"

namespace endian = llvm::support::endian;



// -----------------------------------------------------------------------------
template<typename T> T BitcodeReader::ReadValue()
{
  if (offset_ + sizeof(T) > buf_.getBufferSize()) {
    llvm::report_fatal_error("invalid bitcode file");
  }

  auto *data = buf_.getBufferStart() + offset_;
  offset_ += sizeof(T);
  return endian::read<T, llvm::support::little, 1>(data);
}

// -----------------------------------------------------------------------------
std::string BitcodeReader::ReadString()
{
  uint32_t size = ReadValue<uint32_t>();
  const char *ptr = buf_.getBufferStart();
  std::string s(ptr + offset_, ptr + offset_ + size);
  offset_ += size;
  return s;
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> BitcodeReader::Read()
{
  // Check the magic.
  if (ReadValue<uint32_t>() != kBitcodeMagic) {
    llvm::report_fatal_error("invalid bitcode magic");
  }

  // Read all symbols and their names.
  auto prog = std::make_unique<Prog>();
  {
    // Externs.
    for (unsigned i = 0, n = ReadValue<uint32_t>(); i < n; ++i) {
      globals_.push_back(prog->GetGlobal(ReadString()));
    }

    // Data segments and atoms.
    for (unsigned i = 0, n = ReadValue<uint32_t>(); i < n; ++i) {
      Data *data = prog->CreateData(ReadString());
      for (unsigned j = 0, m = ReadValue<uint32_t>(); j < m; ++j) {
        globals_.push_back(data->CreateAtom(ReadString()));
      }
    }

    // Functions.
    for (unsigned i = 0, n = ReadValue<uint32_t>(); i < n; ++i) {
      Func *func = prog->CreateFunc(ReadString());
      globals_.push_back(func);
      for (unsigned j = 0, m = ReadValue<uint32_t>(); j < m; ++j) {
        Block *block = new Block(ReadString());
        globals_.push_back(block);
        func->AddBlock(block);
      }
    }
  }

  // Read all data items.
  for (Data &data : prog->data()) {
    Read(data);
  }

  // Read all functions.
  for (Func &func : *prog) {
    Read(func);
  }

  return std::move(prog);
}

// -----------------------------------------------------------------------------
void BitcodeReader::Read(Func &func)
{
  func.SetAlignment(ReadValue<uint8_t>());
  func.SetVisibility(static_cast<Visibility>(ReadValue<uint8_t>()));
  func.SetVarArg(ReadValue<uint8_t>());

  // Read stack objects.
  {
    for (unsigned i = 0, n = ReadValue<uint16_t>(); i < n; ++i) {
      auto Index = ReadValue<uint16_t>();
      auto Size = ReadValue<uint32_t>();
      auto Alignment = ReadValue<uint8_t>();
      func.AddStackObject(Index, Size, Alignment);
    }
  }

  // Read parameters.
  {
    std::vector<Type> parameters;
    for (unsigned i = 0, n = ReadValue<uint8_t>(); i < n; ++i) {
      parameters.push_back(static_cast<Type>(ReadValue<uint8_t>()));
    }
    func.SetParameters(parameters);
  }

  // Read blocks.
  {
    std::vector<Inst *> map;
    std::vector<std::tuple<PhiInst *, Global *, unsigned>> fixups;
    for (Block &block : func) {
      for (unsigned i = 0, n = ReadValue<uint32_t>(); i < n; ++i) {
        Inst *inst = ReadInst(map, fixups);
        block.AddInst(inst);
        map.push_back(inst);
      }
    }
    for (auto &[phi, block, idx] : fixups) {
      llvm_unreachable("not implemented");
    }
  }
}

// -----------------------------------------------------------------------------
void BitcodeReader::Read(Data &data)
{
  for (Atom &atom : data) {
    atom.SetAlignment(ReadValue<uint8_t>());
    for (unsigned i = 0, n = ReadValue<uint32_t>(); i < n; ++i) {
      switch (static_cast<Item::Kind>(ReadValue<uint8_t>())) {
        case Item::Kind::INT8: {
          atom.AddInt8(ReadValue<uint8_t>());
          continue;
        }
        case Item::Kind::INT16: {
          atom.AddInt16(ReadValue<uint16_t>());
          continue;
        }
        case Item::Kind::INT32: {
          atom.AddInt32(ReadValue<uint32_t>());
          continue;
        }
        case Item::Kind::INT64: {
          atom.AddInt64(ReadValue<uint64_t>());
          continue;
        }
        case Item::Kind::FLOAT64: {
          atom.AddFloat64(ReadValue<double>());
          continue;
        }
        case Item::Kind::EXPR: {
          atom.AddExpr(ReadExpr());
          continue;
        }
        case Item::Kind::ALIGN: {
          atom.AddAlignment(ReadValue<uint8_t>());
          continue;
        }
        case Item::Kind::SPACE: {
          atom.AddSpace(ReadValue<uint32_t>());
          continue;
        }
        case Item::Kind::STRING: {
          atom.AddString(ReadString());
          continue;
        }
        case Item::Kind::END: {
          atom.AddEnd();
          continue;
        }
      }
      llvm_unreachable("invalid item kind");
    }
  }
}

// -----------------------------------------------------------------------------
Inst *BitcodeReader::ReadInst(
      const std::vector<Inst*> &map,
      std::vector<std::tuple<PhiInst *, Global *, unsigned>> &fixups)
{
  // Parse annotations.
  AnnotSet annot;
  for (unsigned i = 0, mask = ReadValue<uint32_t>(); i < 32; ++i) {
    if (mask & (1 << i)) {
      annot.Set(static_cast<Annot>(i));
    }
  }

  // Parse the number of operands.
  llvm::SmallVector<Value *, 5> values;
  for (unsigned i = 0, n = ReadValue<uint8_t>(); i < n; ++i) {
    switch (static_cast<Value::Kind>(ReadValue<uint8_t>())) {
      case Value::Kind::INST: {
        uint32_t index = ReadValue<uint32_t>();
        if (index >= map.size()) {
          llvm::report_fatal_error("invalid instruction index");
        }
        values.push_back(map[index]);
        continue;
      }
      case Value::Kind::GLOBAL: {
        uint32_t index = ReadValue<uint32_t>();
        if (index >= globals_.size()) {
          llvm::report_fatal_error("invalid global index");
        }
        values.push_back(globals_[index]);
        continue;
      }
      case Value::Kind::EXPR: {
        values.push_back(ReadExpr());
        continue;
      }
      case Value::Kind::CONST: {
        switch (static_cast<Constant::Kind>(ReadValue<uint8_t>())) {
          case Constant::Kind::INT: {
            auto v = ReadValue<int64_t>();
            values.push_back(new ConstantInt(v));
            continue;
          }
          case Constant::Kind::FLOAT: {
            auto v = ReadValue<double>();
            values.push_back(new ConstantFloat(v));
            continue;
          }
          case Constant::Kind::REG: {
            auto v = static_cast<ConstantReg::Kind>(ReadValue<uint8_t>());
            values.push_back(new ConstantReg(v));
            continue;
          }
        }
        llvm_unreachable("invalid constant kind");
      }
    }
    llvm_unreachable("invalid value kind");
  }

  // Helpers to fetch arguments.
  auto value = [&values] (int index) {
    if (index > values.size()) {
      llvm::report_fatal_error("missing argument");
    }
    return values[index];
  };
  auto inst = [&value] (int index) {
    if (auto *inst = ::dyn_cast_or_null<Inst>(value(index))) {
      return inst;
    }
    llvm::report_fatal_error("not an instruction");
  };
  auto bb = [&value] (int index) {
    if (auto *block = ::dyn_cast_or_null<Block>(value(index))) {
      return block;
    }
    llvm::report_fatal_error("not a block");
  };
  auto imm = [&value] (int index) {
    if (auto *imm = ::dyn_cast_or_null<ConstantInt>(value(index))) {
      return imm;
    }
    llvm::report_fatal_error("not an integer");
  };
  auto args = [&values](int beg, int end) {
    std::vector<Inst *> args;
    for (auto it = values.begin() + beg; it != values.end() + end; ++it) {
      if (auto *inst = ::dyn_cast_or_null<Inst>(*it)) {
        args.push_back(inst);
        continue;
      }
      llvm::report_fatal_error("not an instruction");
    }
    return args;
  };

  // Parse the type, if it exists.
  std::optional<Type> ty;
  if (auto val = ReadValue<uint8_t>()) {
    ty = static_cast<Type>(val - 1);
  }
  auto type = [&ty] () -> Type {
    if (!ty) {
      llvm::report_fatal_error("missing type");
    }
    return *ty;
  };

  // Reader for the size.
  auto size = [this] { return ReadValue<uint8_t>(); };

  // Decode the rest.
  auto kind = static_cast<Inst::Kind>(ReadValue<uint8_t>());
  switch (kind) {
    case Inst::Kind::CALL:{
      auto cc = static_cast<CallingConv>(ReadValue<uint8_t>());
      auto size = ReadValue<uint8_t>();
      return new CallInst(ty, inst(0), args(1, 0), size, cc, annot);
    }
    case Inst::Kind::TCALL: {
      auto cc = static_cast<CallingConv>(ReadValue<uint8_t>());
      auto size = ReadValue<uint8_t>();
      return new TailCallInst(ty, inst(0), args(1, 0), size, cc, annot);
    }
    case Inst::Kind::CMP: {
      auto cc = static_cast<Cond>(ReadValue<uint8_t>());
      return new CmpInst(type(), cc, inst(0), inst(1), annot);
    }
    case Inst::Kind::INVOKE: llvm_unreachable("INVOKE");
    case Inst::Kind::TINVOKE: llvm_unreachable("TINVOKE");
    case Inst::Kind::RET: llvm_unreachable("RET");
    case Inst::Kind::JCC: return new JumpCondInst(inst(0), bb(1), bb(2), annot);
    case Inst::Kind::JI: llvm_unreachable("JI");
    case Inst::Kind::JMP: llvm_unreachable("JMP");
    case Inst::Kind::SWITCH: llvm_unreachable("SWITCH");
    case Inst::Kind::TRAP: llvm_unreachable("TRAP");
    case Inst::Kind::LD: return new LoadInst(size(), type(), inst(0), annot);
    case Inst::Kind::ST: return new StoreInst(size(), inst(0), inst(1), annot);
    case Inst::Kind::MOV: return new MovInst(type(), value(0), annot);
    case Inst::Kind::FRAME: return new FrameInst(type(), imm(0), imm(1), annot);
    case Inst::Kind::XCHG: llvm_unreachable("XCHG");
    case Inst::Kind::SET: llvm_unreachable("SET");
    case Inst::Kind::VASTART: llvm_unreachable("VASTART");
    case Inst::Kind::ALLOCA: llvm_unreachable("ALLOCA");
    case Inst::Kind::ARG: llvm_unreachable("ARG");
    case Inst::Kind::UNDEF: llvm_unreachable("UNDEF");
    case Inst::Kind::RDTSC: llvm_unreachable("RDTSC");
    case Inst::Kind::SELECT: llvm_unreachable("SELECT");

    case Inst::Kind::ABS:     return new AbsInst(type(), inst(0), annot);
    case Inst::Kind::NEG:     return new NegInst(type(), inst(0), annot);
    case Inst::Kind::SQRT:    return new SqrtInst(type(), inst(0), annot);
    case Inst::Kind::SIN:     return new SinInst(type(), inst(0), annot);
    case Inst::Kind::COS:     return new CosInst(type(), inst(0), annot);
    case Inst::Kind::SEXT:    return new SExtInst(type(), inst(0), annot);
    case Inst::Kind::ZEXT:    return new ZExtInst(type(), inst(0), annot);
    case Inst::Kind::FEXT:    return new FExtInst(type(), inst(0), annot);
    case Inst::Kind::TRUNC:   return new TruncInst(type(), inst(0), annot);
    case Inst::Kind::EXP:     return new ExpInst(type(), inst(0), annot);
    case Inst::Kind::EXP2:    return new Exp2Inst(type(), inst(0), annot);
    case Inst::Kind::LOG:     return new LogInst(type(), inst(0), annot);
    case Inst::Kind::LOG2:    return new Log2Inst(type(), inst(0), annot);
    case Inst::Kind::LOG10:   return new Log10Inst(type(), inst(0), annot);
    case Inst::Kind::FCEIL:   return new FCeilInst(type(), inst(0), annot);
    case Inst::Kind::FFLOOR:  return new FFloorInst(type(), inst(0), annot);
    case Inst::Kind::POPCNT:  return new PopCountInst(type(), inst(0), annot);
    case Inst::Kind::CLZ:     return new CLZInst(type(), inst(0), annot);

    case Inst::Kind::ADD: llvm_unreachable("ADD");
    case Inst::Kind::AND: llvm_unreachable("AND");
    case Inst::Kind::DIV: llvm_unreachable("DIV");
    case Inst::Kind::REM: llvm_unreachable("REM");
    case Inst::Kind::MUL: llvm_unreachable("MUL");
    case Inst::Kind::OR: llvm_unreachable("OR");
    case Inst::Kind::ROTL: llvm_unreachable("ROTL");
    case Inst::Kind::ROTR: llvm_unreachable("ROTR");
    case Inst::Kind::SLL: llvm_unreachable("SLL");
    case Inst::Kind::SRA: llvm_unreachable("SRA");
    case Inst::Kind::SRL: llvm_unreachable("SRL");
    case Inst::Kind::SUB: llvm_unreachable("SUB");
    case Inst::Kind::XOR: llvm_unreachable("XOR");
    case Inst::Kind::POW: llvm_unreachable("POW");
    case Inst::Kind::COPYSIGN: llvm_unreachable("COPYSIGN");
    case Inst::Kind::UADDO: llvm_unreachable("UADDO");
    case Inst::Kind::UMULO: llvm_unreachable("UMULO");
    case Inst::Kind::USUBO: llvm_unreachable("USUBO");
    case Inst::Kind::SADDO: llvm_unreachable("SADDO");
    case Inst::Kind::SMULO: llvm_unreachable("SMULO");
    case Inst::Kind::SSUBO: llvm_unreachable("SSUBO");

    case Inst::Kind::PHI: llvm_unreachable("PHI");
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
Expr *BitcodeReader::ReadExpr()
{
  switch (static_cast<Expr::Kind>(ReadValue<uint8_t>())) {
    case Expr::Kind::SYMBOL_OFFSET: {
      Global *global;
      if (auto index = ReadValue<uint32_t>()) {
        if (index - 1 >= globals_.size()) {
          llvm::report_fatal_error("invalid global index");
        }
        global = globals_[index - 1];
      } else {
        global = nullptr;
      }
      auto offset = ReadValue<int64_t>();
      return new SymbolOffsetExpr(global, offset);
    }
  }
  llvm_unreachable("invalid expression kind");
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
void BitcodeWriter::Write(const Prog &prog)
{
  // Write the header.
  Emit<uint32_t>(kBitcodeMagic);

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
      for (const Atom &atom : data) {
        Emit(atom.getName());
        symbols_.emplace(&atom, symbols_.size());
      }
    }

    // Functions.
    Emit<uint32_t>(prog.size());
    for (const Func &func : prog.funcs()) {
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
  for (const Data &data : prog.data()) {
    Write(data);
  }

  // Emit all functions.
  for (const Func &func : prog) {
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
  Emit<uint16_t>(objects.size());
  for (const Func::StackObject &obj : objects) {
    Emit<uint16_t>(obj.Index);
    Emit<uint32_t>(obj.Size);
    Emit<uint8_t>(obj.Alignment);
  }

  llvm::ArrayRef<Type> params = func.params();
  Emit<uint8_t>(params.size());
  for (Type type : params) {
    Emit<uint8_t>(static_cast<uint8_t>(type));
  }

  std::unordered_map<const Inst *, unsigned> map;
  for (const Block &block : func) {
    for (const Inst &inst : block) {
      map.emplace(&inst, map.size());
    }
  }

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
  for (const Atom &atom : data) {
    Emit<uint8_t>(atom.GetAlignment());
    Emit<uint32_t>(atom.size());
    for (const Item &item : atom) {
      Item::Kind kind = item.GetKind();
      Emit<uint8_t>(static_cast<uint8_t>(kind));
      switch (kind) {
        case Item::Kind::INT8: {
          Emit<uint8_t>(item.GetInt8());
          continue;
        }
        case Item::Kind::INT16: {
          Emit<uint16_t>(item.GetInt16());
          continue;
        }
        case Item::Kind::INT32: {
          Emit<uint32_t>(item.GetInt32());
          continue;
        }
        case Item::Kind::INT64: {
          Emit<uint64_t>(item.GetInt64());
          continue;
        }
        case Item::Kind::FLOAT64: {
          Emit<double>(item.GetFloat64());
          continue;
        }
        case Item::Kind::EXPR: {
          Write(item.GetExpr());
          continue;
        }
        case Item::Kind::ALIGN: {
          Emit<uint8_t>(item.GetAlign());
          continue;
        }
        case Item::Kind::SPACE: {
          Emit<uint32_t>(item.GetSpace());
          continue;
        }
        case Item::Kind::STRING: {
          Emit(item.GetString());
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
  // Emit the annotations.
  uint32_t annots = 0;
  for (const auto &annot : inst.annots()) {
    annots |= (1 << static_cast<uint32_t>(annot));
  }
  Emit<uint32_t>(annots);

  // Emit the operands.
  Emit<uint8_t>(inst.size());
  for (const Value *value : inst.operand_values()) {
    auto valueKind = value->GetKind();
    Emit<uint8_t>(static_cast<uint8_t>(valueKind));
    switch (valueKind) {
      case Value::Kind::INST: {
        auto it = map.find(static_cast<const Inst *>(value));
        assert(it != map.end() && "missing instruction");
        Emit<uint32_t>(it->second);
        continue;
      }
      case Value::Kind::GLOBAL: {
        auto it = symbols_.find(static_cast<const Global *>(value));
        assert(it != symbols_.end() && "missing symbol");
        Emit<uint32_t>(it->second);
        continue;
      }
      case Value::Kind::EXPR: {
        Write(static_cast<const Expr *>(value));
        continue;
      }
      case Value::Kind::CONST: {
        auto constKind = static_cast<const Constant *>(value)->GetKind();
        Emit<uint8_t>(static_cast<uint8_t>(constKind));
        switch (constKind) {
          case Constant::Kind::INT: {
            auto v = static_cast<const ConstantInt *>(value)->GetInt();
            Emit<int64_t>(v);
            continue;
          }
          case Constant::Kind::FLOAT: {
            auto v = static_cast<const ConstantFloat *>(value)->GetDouble();
            Emit<double>(v);
            continue;
          }
          case Constant::Kind::REG: {
            auto v = static_cast<const ConstantReg *>(value)->GetValue();
            Emit<uint8_t>(static_cast<uint8_t>(v));
            continue;
          }
        }
        llvm_unreachable("invalid constant kind");
      }
    }
    llvm_unreachable("invalid value kind");
  }

  if (inst.GetNumRets() > 0) {
    Emit<uint8_t>(static_cast<uint8_t>(inst.GetType(0)) + 1);
  } else {
    Emit<uint8_t>(0);
  }

  Emit<uint8_t>(static_cast<uint8_t>(inst.GetKind()));
  switch (inst.GetKind()) {
    case Inst::Kind::CALL: {
      auto &call = static_cast<const CallInst &>(inst);
      Emit<uint8_t>(static_cast<uint8_t>(call.GetCallingConv()));
      Emit<uint8_t>(call.GetNumFixedArgs());
      break;
    }
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE:
    case Inst::Kind::TINVOKE: {
      auto &call = static_cast<const CallSite<TerminatorInst> &>(inst);
      Emit<uint8_t>(static_cast<uint8_t>(call.GetCallingConv()));
      Emit<uint8_t>(call.GetNumFixedArgs());
      break;
    }
    case Inst::Kind::CMP: {
      auto &cmp = static_cast<const CmpInst &>(inst);
      Emit<uint8_t>(static_cast<uint8_t>(cmp.GetCC()));
      break;
    }
    case Inst::Kind::LD: {
      auto &load = static_cast<const LoadInst &>(inst);
      Emit<uint8_t>(load.GetLoadSize());
      break;
    }
    case Inst::Kind::ST: {
      auto &store = static_cast<const StoreInst &>(inst);
      Emit<uint8_t>(store.GetStoreSize());
      break;
    }
    default: {
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Expr *expr)
{
  Emit<uint8_t>(static_cast<uint8_t>(expr->GetKind()));
  switch (expr->GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto *offsetExpr = static_cast<const SymbolOffsetExpr *>(expr);
      if (auto *symbol = offsetExpr->GetSymbol()) {
        auto it = symbols_.find(symbol);
        assert(it != symbols_.end() && "missing symbol");
        Emit<uint32_t>(it->second + 1);
      } else {
        Emit<uint32_t>(0);
      }
      Emit<int64_t>(offsetExpr->GetOffset());
      return;
    }
  }
  llvm_unreachable("invalid expression kind");
}
