// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Endian.h>
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

namespace endian = llvm::support::endian;



// -----------------------------------------------------------------------------
template<typename T> T BitcodeReader::ReadData()
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
  uint32_t size = ReadData<uint32_t>();
  const char *ptr = buf_.getBufferStart();
  std::string s(ptr + offset_, ptr + offset_ + size);
  offset_ += size;
  return s;
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> BitcodeReader::Read()
{
  // Check the magic.
  if (ReadData<uint32_t>() != kBitcodeMagic) {
    llvm::report_fatal_error("invalid bitcode magic");
  }

  // Read all symbols and their names.
  auto prog = std::make_unique<Prog>();
  {
    // Externs.
    for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
      globals_.push_back(prog->GetGlobal(ReadString()));
    }

    // Data segments and atoms.
    for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
      Data *data = prog->CreateData(ReadString());
      for (unsigned j = 0, m = ReadData<uint32_t>(); j < m; ++j) {
        globals_.push_back(data->CreateAtom(ReadString()));
      }
    }

    // Functions.
    for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
      Func *func = prog->CreateFunc(ReadString());
      globals_.push_back(func);
      for (unsigned j = 0, m = ReadData<uint32_t>(); j < m; ++j) {
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
  func.SetAlignment(ReadData<uint8_t>());
  func.SetVisibility(static_cast<Visibility>(ReadData<uint8_t>()));
  func.SetCallingConv(static_cast<CallingConv>(ReadData<uint8_t>()));
  func.SetVarArg(ReadData<uint8_t>());

  // Read stack objects.
  {
    for (unsigned i = 0, n = ReadData<uint16_t>(); i < n; ++i) {
      auto Index = ReadData<uint16_t>();
      auto Size = ReadData<uint32_t>();
      auto Alignment = ReadData<uint8_t>();
      func.AddStackObject(Index, Size, Alignment);
    }
  }

  // Read parameters.
  {
    std::vector<Type> parameters;
    for (unsigned i = 0, n = ReadData<uint8_t>(); i < n; ++i) {
      parameters.push_back(static_cast<Type>(ReadData<uint8_t>()));
    }
    func.SetParameters(parameters);
  }

  // Read blocks.
  {
    std::vector<Inst *> map;
    std::vector<std::tuple<PhiInst *, Block *, unsigned>> fixups;
    for (Block &block : func) {
      for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
        Inst *inst = ReadInst(map, fixups);
        block.AddInst(inst);
        map.push_back(inst);
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
void BitcodeReader::Read(Data &data)
{
  for (Atom &atom : data) {
    atom.SetAlignment(ReadData<uint8_t>());
    for (unsigned i = 0, n = ReadData<uint32_t>(); i < n; ++i) {
      switch (static_cast<Item::Kind>(ReadData<uint8_t>())) {
        case Item::Kind::INT8: {
          atom.AddInt8(ReadData<uint8_t>());
          continue;
        }
        case Item::Kind::INT16: {
          atom.AddInt16(ReadData<uint16_t>());
          continue;
        }
        case Item::Kind::INT32: {
          atom.AddInt32(ReadData<uint32_t>());
          continue;
        }
        case Item::Kind::INT64: {
          atom.AddInt64(ReadData<uint64_t>());
          continue;
        }
        case Item::Kind::FLOAT64: {
          atom.AddFloat64(ReadData<double>());
          continue;
        }
        case Item::Kind::EXPR: {
          atom.AddExpr(ReadExpr());
          continue;
        }
        case Item::Kind::ALIGN: {
          atom.AddAlignment(ReadData<uint8_t>());
          continue;
        }
        case Item::Kind::SPACE: {
          atom.AddSpace(ReadData<uint32_t>());
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
      std::vector<std::tuple<PhiInst *, Block *, unsigned>> &fixups)
{
  // Parse annotations.
  AnnotSet annot;
  for (unsigned i = 0, mask = ReadData<uint32_t>(); i < 32; ++i) {
    if (mask & (1 << i)) {
      annot.Set(static_cast<Annot>(i));
    }
  }

  // Parse the type, if it exists.
  std::optional<Type> ty;
  if (auto val = ReadData<uint8_t>()) {
    ty = static_cast<Type>(val - 1);
  }
  auto type = [&ty] () -> Type {
    if (!ty) {
      llvm::report_fatal_error("missing type");
    }
    return *ty;
  };

  auto kind = static_cast<Inst::Kind>(ReadData<uint8_t>());
  if (kind == Inst::Kind::PHI) {
    auto n = ReadData<uint8_t>();
    if (n & 1) {
      llvm::report_fatal_error("invalid number of args for PHI");
    }
    PhiInst *phi = new PhiInst(type(), annot);
    for (unsigned i = 0; i < n; i += 2) {
      if (auto *block = ::dyn_cast_or_null<Block>(ReadValue(map))) {
        switch (static_cast<Value::Kind>(ReadData<uint8_t>())) {
          case Value::Kind::INST: {
            uint32_t index = ReadData<uint32_t>();
            if (index >= map.size()) {
              fixups.emplace_back(phi, block, index);
            } else {
              phi->Add(block, map[index]);
            }
            continue;
          }
          case Value::Kind::GLOBAL: {
            uint32_t index = ReadData<uint32_t>();
            if (index >= globals_.size()) {
              llvm::report_fatal_error("invalid global index");
            }
            phi->Add(block, globals_[index]);
            continue;
          }
          case Value::Kind::EXPR: {
            phi->Add(block, ReadExpr());
            continue;
          }
          case Value::Kind::CONST: {
            switch (static_cast<Constant::Kind>(ReadData<uint8_t>())) {
              case Constant::Kind::INT: {
                auto v = ReadData<int64_t>();
                phi->Add(block, new ConstantInt(v));
                continue;
              }
              case Constant::Kind::FLOAT: {
                auto v = ReadData<double>();
                phi->Add(block, new ConstantFloat(v));
                continue;
              }
              case Constant::Kind::REG: {
                auto v = static_cast<ConstantReg::Kind>(ReadData<uint8_t>());
                phi->Add(block, new ConstantReg(v));
                continue;
              }
            }
            llvm_unreachable("invalid constant kind");
          }
        }
        llvm_unreachable("invalid value kind");
      }
      llvm::report_fatal_error("invalid value type");
    }
    return phi;
  }

  // Parse the number of operands.
  llvm::SmallVector<Value *, 5> values;
  for (unsigned i = 0, n = ReadData<uint8_t>(); i < n; ++i) {
    values.push_back(ReadValue(map));
  }

  // Helpers to fetch arguments.
  auto value = [&values] (int index) {
    if (index >= 0) {
      if (index > values.size()) {
        llvm::report_fatal_error("missing argument");
      }
      return values[index];
    } else {
      if (static_cast<int>(values.size()) + index < 0) {
        llvm::report_fatal_error("missing argument");
      }
      return values[values.size() + index];
    }
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
  auto reg = [&value] (int index) {
    if (auto *reg = ::dyn_cast_or_null<ConstantReg>(value(index))) {
      return reg;
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
  auto blocks = [&values](int beg, int end) {
    std::vector<Block *> blocks;
    for (auto it = values.begin() + beg; it != values.end() + end; ++it) {
      if (auto *inst = ::dyn_cast_or_null<Block>(*it)) {
        blocks.push_back(inst);
        continue;
      }
      llvm::report_fatal_error("not an instruction");
    }
    return blocks;
  };

  // Reader for the size.
  auto size = [this] { return ReadData<uint8_t>(); };

  // Decode the rest.
  switch (kind) {
    case Inst::Kind::CALL: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadData<uint8_t>();
      return new CallInst(ty, inst(0), args(1, 0), size, cc, annot);
    }
    case Inst::Kind::TCALL: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadData<uint8_t>();
      return new TailCallInst(ty, inst(0), args(1, 0), size, cc, annot);
    }
    case Inst::Kind::INVOKE: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadData<uint8_t>();
      return new InvokeInst(ty, inst(0), args(1, -2), bb(-2), bb(-1), size, cc, annot);
    }
    case Inst::Kind::TINVOKE: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadData<uint8_t>();
      return new TailInvokeInst(ty, inst(0), args(1, -1), bb(-1), size, cc, annot);
    }
    // Control flow.
    case Inst::Kind::SWITCH: return new SwitchInst(inst(0), blocks(1, 0), annot);
    case Inst::Kind::JCC: return new JumpCondInst(inst(0), bb(1), bb(2), annot);
    case Inst::Kind::JI: return new JumpIndirectInst(inst(0), annot);
    case Inst::Kind::JMP: return new JumpInst(bb(0), annot);
    case Inst::Kind::TRAP: return new TrapInst(annot);
    case Inst::Kind::RET: {
      if (values.size() == 1) {
        return new ReturnInst(inst(0), annot);
      } else {
        return new ReturnInst(annot);
      }
    }
    // Comparison.
    case Inst::Kind::CMP: {
      auto cc = static_cast<Cond>(ReadData<uint8_t>());
      return new CmpInst(type(), cc, inst(0), inst(1), annot);
    }
    // Memory.
    case Inst::Kind::LD:        return new LoadInst(size(), type(), inst(0), annot);
    case Inst::Kind::ST:        return new StoreInst(size(), inst(0), inst(1), annot);
    case Inst::Kind::XCHG:      return new ExchangeInst(type(), inst(0), inst(1), annot);
    // Constants.
    case Inst::Kind::MOV:       return new MovInst(type(), value(0), annot);
    case Inst::Kind::FRAME:     return new FrameInst(type(), imm(0), imm(1), annot);
    case Inst::Kind::ARG:       return new ArgInst(type(), imm(0), annot);
    case Inst::Kind::UNDEF:     return new UndefInst(type(), annot);
    // Special instructions.
    case Inst::Kind::SELECT:    return new SelectInst(type(), inst(0), inst(1), inst(2), annot);
    case Inst::Kind::RDTSC:     return new RdtscInst(type(), annot);
    case Inst::Kind::VASTART:   return new VAStartInst(inst(0), annot);
    case Inst::Kind::ALLOCA:    return new AllocaInst(type(), inst(0), imm(1), annot);
    case Inst::Kind::SET:       return new SetInst(reg(0), inst(1), annot);
    // Unary instructions.
    case Inst::Kind::ABS:       return new AbsInst(type(), inst(0), annot);
    case Inst::Kind::NEG:       return new NegInst(type(), inst(0), annot);
    case Inst::Kind::SQRT:      return new SqrtInst(type(), inst(0), annot);
    case Inst::Kind::SIN:       return new SinInst(type(), inst(0), annot);
    case Inst::Kind::COS:       return new CosInst(type(), inst(0), annot);
    case Inst::Kind::SEXT:      return new SExtInst(type(), inst(0), annot);
    case Inst::Kind::ZEXT:      return new ZExtInst(type(), inst(0), annot);
    case Inst::Kind::FEXT:      return new FExtInst(type(), inst(0), annot);
    case Inst::Kind::TRUNC:     return new TruncInst(type(), inst(0), annot);
    case Inst::Kind::EXP:       return new ExpInst(type(), inst(0), annot);
    case Inst::Kind::EXP2:      return new Exp2Inst(type(), inst(0), annot);
    case Inst::Kind::LOG:       return new LogInst(type(), inst(0), annot);
    case Inst::Kind::LOG2:      return new Log2Inst(type(), inst(0), annot);
    case Inst::Kind::LOG10:     return new Log10Inst(type(), inst(0), annot);
    case Inst::Kind::FCEIL:     return new FCeilInst(type(), inst(0), annot);
    case Inst::Kind::FFLOOR:    return new FFloorInst(type(), inst(0), annot);
    case Inst::Kind::POPCNT:    return new PopCountInst(type(), inst(0), annot);
    case Inst::Kind::CLZ:       return new CLZInst(type(), inst(0), annot);
    // Binary instructions.
    case Inst::Kind::ADD:       return new AddInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::AND:       return new AndInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::DIV:       return new DivInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::REM:       return new RemInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::MUL:       return new MulInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::OR:        return new OrInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::ROTL:      return new RotlInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::ROTR:      return new RotrInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::SLL:       return new SllInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::SRA:       return new SraInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::SRL:       return new SrlInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::SUB:       return new SubInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::XOR:       return new XorInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::POW:       return new PowInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::COPYSIGN:  return new CopySignInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::UADDO:     return new AddUOInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::UMULO:     return new MulUOInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::USUBO:     return new SubUOInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::SADDO:     return new AddSOInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::SMULO:     return new MulSOInst(type(), inst(0), inst(1), annot);
    case Inst::Kind::SSUBO:     return new SubSOInst(type(), inst(0), inst(1), annot);
    // Phi should have been already handled.
    case Inst::Kind::PHI:       llvm_unreachable("PHI handled separately");
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
Value *BitcodeReader::ReadValue(const std::vector<Inst *> &map)
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
      llvm::ReversePostOrderTraversal<const Func *> rpot(&func);
      Emit<uint32_t>(std::distance(rpot.begin(), rpot.end()));
      {
        for (const Block *block : rpot) {
          Emit(block->getName());
          symbols_.emplace(block, symbols_.size());
        }
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
  Emit<uint8_t>(static_cast<uint8_t>(func.GetCallingConv()));
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

  {
    std::unordered_map<const Inst *, unsigned> map;
    llvm::ReversePostOrderTraversal<const Func *> rpot(&func);
    for (const Block *block : rpot) {
      for (const Inst &inst : *block) {
        map.emplace(&inst, map.size());
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

  // Emit the type, if there is one.
  switch (inst.GetKind()) {
    case Inst::Kind::TCALL:
    case Inst::Kind::TINVOKE: {
      auto &call = static_cast<const CallSite<TerminatorInst> &>(inst);
      if (auto type = call.GetType(); type && call.GetNumRets() == 0) {
        Emit<uint8_t>(static_cast<uint8_t>(*type) + 1);
      } else {
        Emit<uint8_t>(0);
      }
      break;
    }
    default: {
      if (inst.GetNumRets() > 0) {
        Emit<uint8_t>(static_cast<uint8_t>(inst.GetType(0)) + 1);
      } else {
        Emit<uint8_t>(0);
      }
      break;
    }
  }

  // Emit the instruction kind.
  Emit<uint8_t>(static_cast<uint8_t>(inst.GetKind()));

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
