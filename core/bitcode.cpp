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
  if (offset_ + sizeof(T) > buf_.size()) {
    llvm::report_fatal_error("invalid bitcode file");
  }

  auto *data = buf_.data() + offset_;
  offset_ += sizeof(T);
  return endian::read<T, llvm::support::little, 1>(data);
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
  if (ReadData<uint32_t>() != kBitcodeMagic) {
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
        Block *block = new Block(ReadString());
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

  return std::move(prog);
}

// -----------------------------------------------------------------------------
void BitcodeReader::Read(Func &func)
{
  func.SetAlignment(llvm::Align(ReadData<uint8_t>()));
  func.SetVisibility(static_cast<Visibility>(ReadData<uint8_t>()));
  func.SetCallingConv(static_cast<CallingConv>(ReadData<uint8_t>()));
  func.SetExported(ReadData<uint8_t>());
  func.SetVarArg(ReadData<uint8_t>());
  func.SetNoInline(ReadData<uint8_t>());

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
    std::vector<Type> parameters;
    for (unsigned i = 0, n = ReadData<uint16_t>(); i < n; ++i) {
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
void BitcodeReader::Read(Atom &atom)
{
  atom.SetAlignment(llvm::Align(ReadData<uint8_t>()));
  atom.SetVisibility(static_cast<Visibility>(ReadData<uint8_t>()));
  atom.SetExported(ReadData<uint8_t>());
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
}

// -----------------------------------------------------------------------------
Inst *BitcodeReader::ReadInst(
      const std::vector<Inst*> &map,
      std::vector<std::tuple<PhiInst *, Block *, unsigned>> &fixups)
{
  // Parse annotations.
  AnnotSet annots;
  for (unsigned i = 0, n = ReadData<uint8_t>(); i < n; ++i) {
    ReadAnnot(annots);
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
    auto n = ReadData<uint16_t>();
    if (n & 1) {
      llvm::report_fatal_error("invalid number of args for PHI");
    }
    PhiInst *phi = new PhiInst(type(), std::move(annots));
    for (unsigned i = 0; i < n; i += 2) {
      if (auto *block = ::dyn_cast_or_null<Block>(ReadValue(map))) {
        auto kind = static_cast<Value::Kind>(ReadData<uint8_t>());
        assert(kind == Value::Kind::INST && "invalid phi argument");
        uint32_t index = ReadData<uint32_t>();
        if (index >= map.size()) {
          fixups.emplace_back(phi, block, index);
        } else {
          phi->Add(block, map[index]);
        }
      } else {
        llvm::report_fatal_error("invalid value type");
      }
    }
    return phi;
  }

  // Parse the number of operands.
  llvm::SmallVector<Value *, 5> values;
  for (unsigned i = 0, n = ReadData<uint16_t>(); i < n; ++i) {
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
      return new CallInst(ty, inst(0), args(1, 0), size, cc, std::move(annots));
    }
    case Inst::Kind::TCALL: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadData<uint8_t>();
      return new TailCallInst(ty, inst(0), args(1, 0), size, cc, std::move(annots));
    }
    case Inst::Kind::INVOKE: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadData<uint8_t>();
      return new InvokeInst(ty, inst(0), args(1, -2), bb(-2), bb(-1), size, cc, std::move(annots));
    }
    case Inst::Kind::TINVOKE: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadData<uint8_t>();
      return new TailInvokeInst(ty, inst(0), args(1, -1), bb(-1), size, cc, std::move(annots));
    }
    case Inst::Kind::SYSCALL: {
      return new SyscallInst(type(), inst(0), args(1, 0), std::move(annots));
    }
    // Control flow.
    case Inst::Kind::SWITCH: return new SwitchInst(inst(0), blocks(1, 0), std::move(annots));
    case Inst::Kind::JCC: return new JumpCondInst(inst(0), bb(1), bb(2), std::move(annots));
    case Inst::Kind::JI: return new JumpIndirectInst(inst(0), std::move(annots));
    case Inst::Kind::JMP: return new JumpInst(bb(0), std::move(annots));
    case Inst::Kind::TRAP: return new TrapInst(std::move(annots));
    case Inst::Kind::RET: {
      if (values.size() == 1) {
        return new ReturnInst(inst(0), std::move(annots));
      } else {
        return new ReturnInst(std::move(annots));
      }
    }
    // Comparison.
    case Inst::Kind::CMP: {
      auto cc = static_cast<Cond>(ReadData<uint8_t>());
      return new CmpInst(type(), cc, inst(0), inst(1), std::move(annots));
    }
    // Memory.
    case Inst::Kind::LD:        return new LoadInst(type(), inst(0), std::move(annots));
    case Inst::Kind::ST:        return new StoreInst(inst(0), inst(1), std::move(annots));
    case Inst::Kind::XCHG:      return new XchgInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::CMPXCHG:   return new CmpXchgInst(type(), inst(0), inst(1), inst(2), std::move(annots));
    // Constants.
    case Inst::Kind::MOV:       return new MovInst(type(), value(0), std::move(annots));
    case Inst::Kind::FRAME:     return new FrameInst(type(), imm(0), imm(1), std::move(annots));
    case Inst::Kind::ARG:       return new ArgInst(type(), imm(0), std::move(annots));
    case Inst::Kind::UNDEF:     return new UndefInst(type(), std::move(annots));
    // Special instructions.
    case Inst::Kind::SELECT:    return new SelectInst(type(), inst(0), inst(1), inst(2), std::move(annots));
    case Inst::Kind::RDTSC:     return new RdtscInst(type(), std::move(annots));
    case Inst::Kind::FNSTCW:    return new FNStCwInst(inst(0), std::move(annots));
    case Inst::Kind::FLDCW:     return new FLdCwInst(inst(0), std::move(annots));
    case Inst::Kind::VASTART:   return new VAStartInst(inst(0), std::move(annots));
    case Inst::Kind::ALLOCA:    return new AllocaInst(type(), inst(0), imm(1), std::move(annots));
    case Inst::Kind::SET:       return new SetInst(reg(0), inst(1), std::move(annots));
    // Unary instructions.
    case Inst::Kind::ABS:       return new AbsInst(type(), inst(0), std::move(annots));
    case Inst::Kind::NEG:       return new NegInst(type(), inst(0), std::move(annots));
    case Inst::Kind::SQRT:      return new SqrtInst(type(), inst(0), std::move(annots));
    case Inst::Kind::SIN:       return new SinInst(type(), inst(0), std::move(annots));
    case Inst::Kind::COS:       return new CosInst(type(), inst(0), std::move(annots));
    case Inst::Kind::SEXT:      return new SExtInst(type(), inst(0), std::move(annots));
    case Inst::Kind::ZEXT:      return new ZExtInst(type(), inst(0), std::move(annots));
    case Inst::Kind::XEXT:      return new XExtInst(type(), inst(0), std::move(annots));
    case Inst::Kind::FEXT:      return new FExtInst(type(), inst(0), std::move(annots));
    case Inst::Kind::TRUNC:     return new TruncInst(type(), inst(0), std::move(annots));
    case Inst::Kind::EXP:       return new ExpInst(type(), inst(0), std::move(annots));
    case Inst::Kind::EXP2:      return new Exp2Inst(type(), inst(0), std::move(annots));
    case Inst::Kind::LOG:       return new LogInst(type(), inst(0), std::move(annots));
    case Inst::Kind::LOG2:      return new Log2Inst(type(), inst(0), std::move(annots));
    case Inst::Kind::LOG10:     return new Log10Inst(type(), inst(0), std::move(annots));
    case Inst::Kind::FCEIL:     return new FCeilInst(type(), inst(0), std::move(annots));
    case Inst::Kind::FFLOOR:    return new FFloorInst(type(), inst(0), std::move(annots));
    case Inst::Kind::POPCNT:    return new PopCountInst(type(), inst(0), std::move(annots));
    case Inst::Kind::CLZ:       return new CLZInst(type(), inst(0), std::move(annots));
    case Inst::Kind::CTZ:       return new CTZInst(type(), inst(0), std::move(annots));
    // Binary instructions.
    case Inst::Kind::ADD:       return new AddInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::AND:       return new AndInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::UDIV:      return new UDivInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::SDIV:      return new SDivInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::UREM:      return new URemInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::SREM:      return new SRemInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::MUL:       return new MulInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::OR:        return new OrInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::ROTL:      return new RotlInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::ROTR:      return new RotrInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::SLL:       return new SllInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::SRA:       return new SraInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::SRL:       return new SrlInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::SUB:       return new SubInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::XOR:       return new XorInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::POW:       return new PowInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::COPYSIGN:  return new CopySignInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::UADDO:     return new AddUOInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::UMULO:     return new MulUOInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::USUBO:     return new SubUOInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::SADDO:     return new AddSOInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::SMULO:     return new MulSOInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::SSUBO:     return new SubSOInst(type(), inst(0), inst(1), std::move(annots));
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
void BitcodeReader::ReadAnnot(AnnotSet &annots)
{
  switch (static_cast<Annot::Kind>(ReadData<uint8_t>())) {
    case Annot::Kind::CAML_VALUE: {
      annots.Set<CamlValue>();
      return;
    }
    case Annot::Kind::CAML_ADDR: {
      annots.Set<CamlAddr>();
      return;
    }
    case Annot::Kind::CAML_FRAME: {
      std::vector<size_t> allocs;
      for (uint8_t i = 0, n = ReadData<uint8_t>(); i < n; ++i) {
        allocs.push_back(ReadData<size_t>());
      }
      bool raise = ReadData<bool>();
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
      annots.Set<CamlFrame>(std::move(allocs), raise, std::move(debug_infos));
      return;
    }
  }
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
          symbols_.emplace(block, symbols_.size());
        }
      }
    }
  }

  // Emit all data items.
  for (const Data &data : prog.data()) {
    for (const Object &object : data) {
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
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Func &func)
{
  // Emit attributes.
  Emit<uint8_t>(static_cast<uint8_t>(func.GetAlignment().value()));
  Emit<uint8_t>(static_cast<uint8_t>(func.GetVisibility()));
  Emit<uint8_t>(static_cast<uint8_t>(func.GetCallingConv()));
  Emit<uint8_t>(func.IsExported());
  Emit<uint8_t>(func.IsVarArg());
  Emit<uint8_t>(func.IsNoInline());

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
    llvm::ArrayRef<Type> params = func.params();
    Emit<uint16_t>(params.size());
    for (Type type : params) {
      Emit<uint8_t>(static_cast<uint8_t>(type));
    }
  }

  // Emit BBs and instructions.
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
void BitcodeWriter::Write(const Atom &atom)
{
  Emit<uint8_t>(atom.GetAlignment().value());
  Emit<uint8_t>(static_cast<uint8_t>(atom.GetVisibility()));
  Emit<uint8_t>(atom.IsExported());
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
      case Item::Kind::EXPR: {
        Write(*item.GetExpr());
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
  if (const Global *g = ext.GetAlias()) {
    auto it = symbols_.find(g);
    assert(it != symbols_.end() && "missing symbol ID");
    Emit<uint32_t>(it->second);
  } else {
    Emit<uint32_t>(0);
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(
    const Inst &inst,
    const std::unordered_map<const Inst *, unsigned> &map)
{
  // Emit the annotations.
  Emit<uint8_t>(inst.annot_size());
  for (const auto &annot : inst.annots()) {
    Write(annot);
  }

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
  Emit<uint16_t>(inst.size());
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
        Write(*static_cast<const Expr *>(value));
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
    default: {
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(const Expr &expr)
{
  Emit<uint8_t>(static_cast<uint8_t>(expr.GetKind()));
  switch (expr.GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto &offsetExpr = static_cast<const SymbolOffsetExpr &>(expr);
      if (auto *symbol = offsetExpr.GetSymbol()) {
        auto it = symbols_.find(symbol);
        assert(it != symbols_.end() && "missing symbol");
        Emit<uint32_t>(it->second + 1);
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
void BitcodeWriter::Write(const Annot &annot)
{
  Emit<uint8_t>(static_cast<uint8_t>(annot.GetKind()));
  switch (annot.GetKind()) {
    case Annot::Kind::CAML_VALUE: {
      return;
    }
    case Annot::Kind::CAML_ADDR: {
      return;
    }
    case Annot::Kind::CAML_FRAME: {
      auto &frame = static_cast<const CamlFrame &>(annot);
      Emit<uint8_t>(frame.alloc_size());
      for (const auto &alloc : frame.allocs()) {
        Emit<size_t>(alloc);
      }
      Emit<bool>(frame.IsRaise());
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
  }
}
