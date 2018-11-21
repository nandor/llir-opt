// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <optional>

#include <llvm/Target/X86/X86ISelLowering.h>
#include <llvm/Target/X86/X86InstrInfo.h>

#include "core/block.h"
#include "core/func.h"
#include "core/insts.h"
#include "emitter/x86/x86call.h"

namespace X86 = llvm::X86;
using MVT = llvm::MVT;



// -----------------------------------------------------------------------------
X86Call::X86Call(const Func *func)
  : stack_(0ull)
  , args_(func->GetNumFixedArgs())
  , regs_(0)
  , xmms_(0)
{
  unsigned nargs = func->GetNumFixedArgs();
  std::vector<std::optional<const ArgInst *>> args(nargs);
  for (const Block &block : *func) {
    for (const Inst &inst : block) {
      if (inst.GetKind() != Inst::Kind::ARG) {
        continue;
      }

      auto &argInst = static_cast<const ArgInst &>(inst);
      if (argInst.GetIdx() >= nargs) {
        throw std::runtime_error("Function declared fewer args");
      }
      args[argInst.GetIdx()] = &argInst;
    }
  }

  for (unsigned i = 0; i < nargs; ++i) {
    if (!args[i]) {
      continue;
    }
    Assign(func->GetCallingConv(), i, (*args[i])->GetType(), *args[i]);
  }
}

// -----------------------------------------------------------------------------
void X86Call::Assign(CallingConv conv, unsigned i, Type type, const Inst *value)
{
  switch (conv) {
    case CallingConv::C:     AssignC(i, type, value); break;
    case CallingConv::FAST:  AssignC(i, type, value); break;
    case CallingConv::OCAML: AssignOCaml(i, type, value); break;
  }
}

// -----------------------------------------------------------------------------
void X86Call::AssignC(unsigned i, Type type, const Inst *value)
{
  static const std::vector<unsigned> kArgI32 = {
    X86::EDI, X86::ESI, X86::EDX,
    X86::ECX, X86::R8D, X86::R9D
  };
  static const std::vector<unsigned> kArgI64 = {
    X86::RDI, X86::RSI, X86::RDX,
    X86::RCX, X86::R8,  X86::R9
  };
  static const std::vector<unsigned> kArgF = {
    X86::XMM0, X86::XMM1, X86::XMM2, X86::XMM3,
    X86::XMM4, X86::XMM5, X86::XMM6, X86::XMM7
  };

  switch (type) {
    case Type::U8:  case Type::I8:
    case Type::U16: case Type::I16: {
      throw std::runtime_error("Invalid argument type");
    }
    case Type::U32: case Type::I32: {
      if (regs_ < kArgI32.size()) {
        AssignReg(i, type, value, kArgI32[regs_]);
      } else {
        assert(!"not implemented");
      }
      break;
    }
    case Type::U64: case Type::I64: {
      if (regs_ < kArgI64.size()) {
        AssignReg(i, type, value, kArgI64[regs_]);
      } else {
        assert(!"not implemented");
      }
      break;
    }
    case Type::F32: case Type::F64: {
      if (xmms_ < kArgF.size()) {
        AssignReg(i, type, value, kArgF[xmms_]);
      } else {
        assert(!"not implemented");
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void X86Call::AssignOCaml(unsigned i, Type type, const Inst *value)
{
  static const std::vector<unsigned> kArgReg = {
    X86::RAX, X86::RBX,
    X86::RDI, X86::RSI,
    X86::RDX, X86::RCX,
    X86::R8,  X86::R9, X86::R12, X86::R13
  };
  static const std::vector<unsigned> kArgF = {
    X86::XMM0, X86::XMM1, X86::XMM2, X86::XMM3,
    X86::XMM4, X86::XMM5, X86::XMM6, X86::XMM7
  };

  switch (type) {
    case Type::U8:  case Type::I8:
    case Type::U16: case Type::I16:
    case Type::U32: case Type::I32: {
      throw std::runtime_error("Invalid argument type");
    }
    case Type::U64: case Type::I64: {
      if (regs_ < kArgReg.size()) {
        AssignReg(i, type, value, kArgReg[regs_]);
      } else {
        assert(!"not implemented");
      }
      break;
    }
    case Type::F32: case Type::F64: {
      if (xmms_ < kArgF.size()) {
        AssignXMM(i, type, value, kArgF[xmms_]);
      } else {
        assert(!"not implemented");
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void X86Call::AssignReg(unsigned i, Type type, const Inst *value, unsigned reg)
{
  args_[i].Index = i;
  args_[i].Kind = Loc::Kind::REG;
  args_[i].Reg = reg;
  args_[i].Type = type;
  args_[i].Value = value;
  regs_++;
}

// -----------------------------------------------------------------------------
void X86Call::AssignXMM(unsigned i, Type type, const Inst *value, unsigned reg)
{
  args_[i].Index = i;
  args_[i].Kind = Loc::Kind::REG;
  args_[i].Reg = reg;
  args_[i].Type = type;
  args_[i].Value = value;
  xmms_++;
}

