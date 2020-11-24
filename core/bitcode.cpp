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
#include "core/xtor.h"

namespace endian = llvm::support::endian;



/**
 * Helper to load data from a buffer.
 */
template<typename T, const uint64_t Magic>
T CheckMagic(llvm::StringRef buffer, uint64_t offset)
{
  namespace endian = llvm::support::endian;
  if (offset + sizeof(T) > buffer.size()) {
    return false;
  }
  auto *data = buffer.data() + offset;
  return endian::read<T, llvm::support::little, 1>(data) == Magic;
}

// -----------------------------------------------------------------------------
bool IsLLIRObject(llvm::StringRef buffer)
{
  return CheckMagic<uint32_t, kLLIRMagic>(buffer, 0);
}

// -----------------------------------------------------------------------------
bool IsLLARArchive(llvm::StringRef buffer)
{
  return CheckMagic<uint32_t, kLLARMagic>(buffer, 0);
}

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
  func.SetAlignment(llvm::Align(ReadData<uint8_t>()));
  func.SetVisibility(static_cast<Visibility>(ReadData<uint8_t>()));
  func.SetCallingConv(static_cast<CallingConv>(ReadData<uint8_t>()));
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
  atom.SetAlignment(llvm::Align(ReadData<uint8_t>()));
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

  // Parse the type, if it exists.
  std::vector<Type> ts;
  for (unsigned i = 0, n = ReadData<uint8_t>(); i < n; ++i) {
    ts.push_back(static_cast<Type>(ReadData<uint8_t>()));
  }
  auto type = [&ts] () -> Type {
    if (ts.empty()) {
      llvm::report_fatal_error("missing type");
    }
    return ts[0];
  };

  auto kind = static_cast<Inst::Kind>(ReadData<uint8_t>());
  if (kind == Inst::Kind::PHI) {
    auto n = ReadData<uint16_t>();
    if (n & 1) {
      llvm::report_fatal_error("invalid number of args for PHI");
    }
    PhiInst *phi = new PhiInst(type(), std::move(annots));
    for (unsigned i = 0; i < n; i += 2) {
      if (Ref<Block> block = ::cast_or_null<Block>(ReadValue(map))) {
        auto kind = static_cast<Value::Kind>(ReadData<uint8_t>());
        assert(kind == Value::Kind::INST && "invalid phi argument");
        uint32_t index = ReadData<uint32_t>();
        if (index >= map.size()) {
          fixups.emplace_back(phi, block.Get(), index);
        } else {
          phi->Add(block.Get(), map[index]);
        }
      } else {
        llvm::report_fatal_error("invalid value type");
      }
    }
    return phi;
  }

  // Parse the number of operands.
  llvm::SmallVector<Ref<Value>, 5> values;
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
    if (auto instRef = ::cast_or_null<Inst>(value(index))) {
      return instRef;
    }
    llvm::report_fatal_error("not an instruction");
  };
  auto bb = [&value] (int index) {
    if (auto blockRef = ::cast_or_null<Block>(value(index))) {
      return blockRef.Get();
    }
    llvm::report_fatal_error("not a block");
  };
  auto imm = [&value] (int index) {
    if (auto immRef = ::cast_or_null<ConstantInt>(value(index))) {
      return immRef.Get();
    }
    llvm::report_fatal_error("not an integer");
  };
  auto reg = [&value] (int index) {
    if (auto regRef = ::cast_or_null<ConstantReg>(value(index))) {
      return regRef;
    }
    llvm::report_fatal_error("not an integer");
  };
  auto args = [&values](int beg, int end) {
    std::vector<Ref<Inst>> args;
    for (auto it = values.begin() + beg; it != values.end() + end; ++it) {
      if (auto instRef = ::cast_or_null<Inst>(*it)) {
        args.emplace_back(std::move(instRef));
        continue;
      }
      llvm::report_fatal_error("not an instruction");
    }
    return args;
  };
  auto blocks = [&values](int beg, int end) {
    std::vector<Block *> blocks;
    for (auto it = values.begin() + beg; it != values.end() + end; ++it) {
      if (auto block = ::cast_or_null<Block>(*it)) {
        blocks.emplace_back(block.Get());
        continue;
      }
      llvm::report_fatal_error("not an instruction");
    }
    return blocks;
  };

  // Decode the rest.
  using K = Inst::Kind;
  switch (kind) {
    case Inst::Kind::CALL: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadOptional<uint16_t>();
      return new CallInst(
          ts,
          inst(0),
          args(1, -1),
          bb(-1),
          size,
          cc,
          std::move(annots)
      );
    }
    case Inst::Kind::TCALL: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadOptional<uint16_t>();
      return new TailCallInst(
          ts,
          inst(0),
          args(1, 0),
          size,
          cc,
          std::move(annots)
      );
    }
    case Inst::Kind::INVOKE: {
      auto cc = static_cast<CallingConv>(ReadData<uint8_t>());
      auto size = ReadOptional<uint16_t>();
      return new InvokeInst(
          ts,
          inst(0),
          args(1, -2),
          bb(-2),
          bb(-1),
          size,
          cc,
          std::move(annots)
      );
    }
    // Hardware instructions.
    case Inst::Kind::SYSCALL: {
      return new SyscallInst(
          ts.empty() ? std::nullopt : std::optional<Type>(ts[0]),
          inst(0),
          args(1, 0),
          std::move(annots)
      );
    }
    case Inst::Kind::CLONE: {
      return new CloneInst(
          type(),
          inst(0),
          inst(1),
          inst(2),
          inst(3),
          inst(4),
          inst(5),
          inst(6),
          std::move(annots)
      );
    }
    case Inst::Kind::RAISE: {
      std::optional<CallingConv> cc = std::nullopt;
      if (unsigned n = ReadData<uint8_t>()) {
        cc = static_cast<CallingConv>(n - 1);
      }
      return new RaiseInst(
          cc,
          inst(0),
          inst(1),
          args(2, 0),
          std::move(annots)
      );
    }
    case Inst::Kind::LANDING_PAD: {
      std::optional<CallingConv> cc = std::nullopt;
      if (unsigned n = ReadData<uint8_t>()) {
        cc = static_cast<CallingConv>(n - 1);
      }
      return new LandingPadInst(cc, ts, std::move(annots));
    }
    // Control flow.
    case Inst::Kind::SWITCH: return new SwitchInst(inst(0), blocks(1, 0), std::move(annots));
    case Inst::Kind::JCC: return new JumpCondInst(inst(0), bb(1), bb(2), std::move(annots));
    case Inst::Kind::JMP: return new JumpInst(bb(0), std::move(annots));
    case Inst::Kind::TRAP: return new TrapInst(std::move(annots));
    case Inst::Kind::RET: return new ReturnInst(args(0, 0), std::move(annots));
    // Comparison.
    case Inst::Kind::CMP: {
      auto cc = static_cast<Cond>(ReadData<uint8_t>());
      return new CmpInst(type(), cc, inst(0), inst(1), std::move(annots));
    }
    // Memory.
    case Inst::Kind::LD:        return new LoadInst(type(), inst(0), std::move(annots));
    case Inst::Kind::ST:        return new StoreInst(inst(0), inst(1), std::move(annots));
    // Constants.
    case Inst::Kind::MOV:       return new MovInst(type(), value(0), std::move(annots));
    case Inst::Kind::FRAME:     return new FrameInst(type(), imm(0), imm(1), std::move(annots));
    case Inst::Kind::ARG:       return new ArgInst(type(), imm(0), std::move(annots));
    case Inst::Kind::UNDEF:     return new UndefInst(type(), std::move(annots));
    // Special instructions.
    case Inst::Kind::SELECT:    return new SelectInst(type(), inst(0), inst(1), inst(2), std::move(annots));
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
    case Inst::Kind::BSWAP:     return new BSwapInst(type(), inst(0), std::move(annots));
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
    // X86 hardware instructions.
    case Inst::Kind::X86_XCHG:      return new X86_XchgInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::X86_CMPXCHG:   return new X86_CmpXchgInst(type(), inst(0), inst(1), inst(2), std::move(annots));
    case Inst::Kind::X86_FNSTCW:    return new X86_FnStCwInst(inst(0), std::move(annots));
    case Inst::Kind::X86_FNSTSW:    return new X86_FnStSwInst(inst(0), std::move(annots));
    case Inst::Kind::X86_FNSTENV:   return new X86_FnStEnvInst(inst(0), std::move(annots));
    case Inst::Kind::X86_FLDCW:     return new X86_FLdCwInst(inst(0), std::move(annots));
    case Inst::Kind::X86_FLDENV:    return new X86_FLdEnvInst(inst(0), std::move(annots));
    case Inst::Kind::X86_LDMXCSR:   return new X86_LdmXCSRInst(inst(0), std::move(annots));
    case Inst::Kind::X86_STMXCSR:   return new X86_StmXCSRInst(inst(0), std::move(annots));
    case Inst::Kind::X86_FNCLEX:    return new X86_FnClExInst(std::move(annots));
    case Inst::Kind::X86_RDTSC:     return new X86_RdtscInst(type(), std::move(annots));
    // AArch64 instructions.
    case Inst::Kind::AARCH64_LL:    return new AArch64_LL(type(), inst(0), std::move(annots));
    case Inst::Kind::AARCH64_SC:    return new AArch64_SC(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::AARCH64_DMB:   return new AArch64_DMB(std::move(annots));
    // RISC-V instructions.
    case Inst::Kind::RISCV_XCHG:    return new RISCV_XchgInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::RISCV_CMPXCHG: return new RISCV_CmpXchgInst(type(), inst(0), inst(1), inst(2), std::move(annots));
    case Inst::Kind::RISCV_FENCE:   return new RISCV_FenceInst(std::move(annots));
    case Inst::Kind::RISCV_GP:      return new RISCV_GPInst(std::move(annots));
    // PowerPC instructions.
    case Inst::Kind::PPC_LL:        return new PPC_LLInst(type(), inst(0), std::move(annots));
    case Inst::Kind::PPC_SC:        return new PPC_SCInst(type(), inst(0), inst(1), std::move(annots));
    case Inst::Kind::PPC_SYNC:      return new PPC_SyncInst(std::move(annots));
    case Inst::Kind::PPC_ISYNC:     return new PPC_ISyncInst(std::move(annots));
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
  Emit<uint8_t>(static_cast<uint8_t>(func.GetAlignment().value()));
  Emit<uint8_t>(static_cast<uint8_t>(func.GetVisibility()));
  Emit<uint8_t>(static_cast<uint8_t>(func.GetCallingConv()));
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
    std::unordered_map<ConstRef<Inst>, unsigned> map;
    llvm::ReversePostOrderTraversal<const Func *> rpot(&func);
    for (const Block *block : rpot) {
      for (const Inst &inst : *block) {
        for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
          map.emplace(ConstRef<Inst>(&inst, i), map.size());
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
  Emit<uint8_t>(atom.GetAlignment().value());
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
  if (auto symbol = ext.GetSection()) {
    Emit<uint8_t>(1);
    Emit(std::string(*symbol));
  } else {
    Emit<uint8_t>(0);
  }
}

// -----------------------------------------------------------------------------
void BitcodeWriter::Write(
    const Inst &inst,
    const std::unordered_map<ConstRef<Inst>, unsigned> &map)
{
  // Emit the annotations.
  Emit<uint8_t>(inst.annot_size());
  for (const auto &annot : inst.annots()) {
    Write(annot);
  }

  // Emit the type, if there is one.
  switch (inst.GetKind()) {
    case Inst::Kind::TCALL: {
      auto &tcall = static_cast<const TailCallInst &>(inst);
      unsigned n = tcall.type_size();
      Emit<uint8_t>(n);
      for (unsigned i = 0; i < n; ++i) {
        Emit<uint8_t>(static_cast<uint8_t>(tcall.type(i)));
      }
      break;
    }
    default: {
      unsigned n = inst.GetNumRets();
      Emit<uint8_t>(n);
      for (unsigned i = 0; i < n; ++i) {
        Emit<uint8_t>(static_cast<uint8_t>(inst.GetType(i)));
      }
      break;
    }
  }

  // Emit the instruction kind.
  Emit<uint8_t>(static_cast<uint8_t>(inst.GetKind()));

  // Emit the operands.
  Emit<uint16_t>(inst.size());
  for (ConstRef<Value> value : inst.operand_values()) {
    auto valueKind = value->GetKind();
    Emit<uint8_t>(static_cast<uint8_t>(valueKind));
    switch (valueKind) {
      case Value::Kind::INST: {
        auto it = map.find(cast<Inst>(value));
        assert(it != map.end() && "missing instruction");
        Emit<uint32_t>(it->second);
        continue;
      }
      case Value::Kind::GLOBAL: {
        auto it = symbols_.find(&*cast<Global>(value));
        assert(it != symbols_.end() && "missing symbol");
        Emit<uint32_t>(it->second);
        continue;
      }
      case Value::Kind::EXPR: {
        Write(*cast<Expr>(value));
        continue;
      }
      case Value::Kind::CONST: {
        auto &c = *cast<Constant>(value);
        auto constKind = c.GetKind();
        Emit<uint8_t>(static_cast<uint8_t>(constKind));
        switch (constKind) {
          case Constant::Kind::INT: {
            auto v = static_cast<const ConstantInt &>(c).GetInt();
            Emit<int64_t>(v);
            continue;
          }
          case Constant::Kind::FLOAT: {
            auto v = static_cast<const ConstantFloat &>(c).GetDouble();
            Emit<double>(v);
            continue;
          }
          case Constant::Kind::REG: {
            auto v = static_cast<const ConstantReg &>(c).GetValue();
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
    case Inst::Kind::CALL:
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE: {
      auto &call = static_cast<const CallSite &>(inst);
      Emit<uint8_t>(static_cast<uint8_t>(call.GetCallingConv()));
      if (auto fixed = call.GetNumFixedArgs()) {
        Emit<uint16_t>(*fixed + 1);
      } else {
        Emit<uint16_t>(0);
      }
      break;
    }
    case Inst::Kind::CMP: {
      auto &cmp = static_cast<const CmpInst &>(inst);
      Emit<uint8_t>(static_cast<uint8_t>(cmp.GetCC()));
      break;
    }
    case Inst::Kind::RAISE: {
      auto &raise = static_cast<const RaiseInst &>(inst);
      if (auto cc = raise.GetCallingConv()) {
        Emit<uint8_t>(static_cast<uint8_t>(*cc) + 1);
      } else {
        Emit<uint8_t>(0);
      }
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
  Emit<uint8_t>(static_cast<uint8_t>(xtor.getKind()));
  Emit<int32_t>(xtor.getPriority());
  auto it = symbols_.find(xtor.getFunc());
  assert(it != symbols_.end() && "missing symbol");
  Emit<uint32_t>(it->second);
}
