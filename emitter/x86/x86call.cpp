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
// Registers used by C and FAST to pass arguments.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kCGPR32 = {
  X86::EDI, X86::ESI, X86::EDX,
  X86::ECX, X86::R8D, X86::R9D
};
static const std::vector<unsigned> kCGPR64 = {
  X86::RDI, X86::RSI, X86::RDX,
  X86::RCX, X86::R8,  X86::R9
};
static const std::vector<unsigned> kCXMM = {
  X86::XMM0, X86::XMM1, X86::XMM2, X86::XMM3,
  X86::XMM4, X86::XMM5, X86::XMM6, X86::XMM7
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to pass arguments.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGPR64 = {
  X86::RAX, X86::RBX,
  X86::RDI, X86::RSI,
  X86::RDX, X86::RCX,
  X86::R8,  X86::R9, X86::R12, X86::R13
};
static const std::vector<unsigned> kOCamlXMM = {
  X86::XMM0, X86::XMM1, X86::XMM2, X86::XMM3,
  X86::XMM4, X86::XMM5, X86::XMM6, X86::XMM7
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to C calls to pass arguments.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kExtGPR64 = {
  X86::RAX,
  X86::RDI, X86::RSI, X86::RDX,
  X86::RCX, X86::R8,  X86::R9
};
static const std::vector<unsigned> kExtXMM = {
  X86::XMM0, X86::XMM1, X86::XMM2, X86::XMM3,
  X86::XMM4, X86::XMM5, X86::XMM6, X86::XMM7
};



// -----------------------------------------------------------------------------
X86Call::X86Call(const Func *func)
  : conv_(func->GetCallingConv())
  , args_(func->GetParameters().size())
  , stack_(0ull)
  , regs_(0)
  , xmms_(0)
{
  const auto &params = func->GetParameters();
  unsigned nargs = params.size();
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
      if (params[argInst.GetIdx()] != argInst.GetType()) {
        throw std::runtime_error("Invalid argument type");
      }
    }
  }

  for (unsigned i = 0; i < nargs; ++i) {
    if (args[i]) {
      Assign(i, (*args[i])->GetType(), nullptr);
    } else {
      Assign(i, params[i], nullptr);
    }
  }
}

// -----------------------------------------------------------------------------
void X86Call::Assign(unsigned i, Type type, const Inst *value)
{
  switch (conv_) {
    case CallingConv::C:     AssignC(i, type, value);     break;
    case CallingConv::FAST:  AssignC(i, type, value);     break;
    case CallingConv::OCAML: AssignOCaml(i, type, value); break;
    case CallingConv::EXT:   AssignExt(i, type, value);   break;
  }
}

// -----------------------------------------------------------------------------
void X86Call::AssignC(unsigned i, Type type, const Inst *value)
{
  switch (type) {
    case Type::U8:  case Type::I8:
    case Type::U16: case Type::I16: {
      throw std::runtime_error("Invalid argument type");
    }
    case Type::U32: case Type::I32: {
      if (regs_ < kCGPR32.size()) {
        AssignReg(i, type, value, kCGPR32[regs_]);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
    case Type::U64: case Type::I64: {
      if (regs_ < kCGPR64.size()) {
        AssignReg(i, type, value, kCGPR64[regs_]);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
    case Type::F32: case Type::F64: {
      if (xmms_ < kCXMM.size()) {
        AssignXMM(i, type, value, kCXMM[xmms_]);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void X86Call::AssignOCaml(unsigned i, Type type, const Inst *value)
{
  switch (type) {
    case Type::U8:  case Type::I8:
    case Type::U16: case Type::I16:
    case Type::U32: case Type::I32: {
      throw std::runtime_error("Invalid argument type");
    }
    case Type::U64: case Type::I64: {
      if (regs_ < kOCamlGPR64.size()) {
        AssignReg(i, type, value, kOCamlGPR64[regs_]);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
    case Type::F32: case Type::F64: {
      if (xmms_ < kOCamlXMM.size()) {
        AssignXMM(i, type, value, kOCamlXMM[xmms_]);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void X86Call::AssignExt(unsigned i, Type type, const Inst *value)
{
  if (i == 0 && (type != Type::I64 && type != Type::U64)) {
    throw std::runtime_error("First argument must be an address");
  }

  switch (type) {
    case Type::U8:  case Type::I8:
    case Type::U16: case Type::I16:
    case Type::U32: case Type::I32: {
      throw std::runtime_error("Invalid argument type");
    }
    case Type::U64: case Type::I64: {
      if (regs_ < kExtGPR64.size()) {
        AssignReg(i, type, value, kExtGPR64[regs_]);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
    case Type::F32: case Type::F64: {
      if (xmms_ < kExtXMM.size()) {
        AssignXMM(i, type, value, kExtXMM[xmms_]);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetUnusedGPRs() const
{
  return GetGPRs().drop_front(regs_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetUsedGPRs() const
{
  return GetGPRs().take_front(regs_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetUnusedXMMs() const
{
  return GetXMMs().drop_front(xmms_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetUsedXMMs() const
{
  return GetXMMs().take_front(xmms_);
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

// -----------------------------------------------------------------------------
void X86Call::AssignStack(unsigned i, Type type, const Inst *value)
{
  args_[i].Index = i;
  args_[i].Kind = Loc::Kind::STK;
  args_[i].Idx = stack_;
  args_[i].Size = 8;
  args_[i].Type = type;
  args_[i].Value = value;

  stack_ = stack_ + 8;
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetGPRs() const
{
  switch (conv_) {
    case CallingConv::C: case CallingConv::FAST: {
      return llvm::ArrayRef<unsigned>(kCGPR64);
    }
    case CallingConv::OCAML: {
      return llvm::ArrayRef<unsigned>(kOCamlGPR64);
    }
    case CallingConv::EXT: {
      return llvm::ArrayRef<unsigned>(kExtGPR64);
    }
  }
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetXMMs() const
{
  switch (conv_) {
    case CallingConv::C: case CallingConv::FAST: {
      return llvm::ArrayRef<unsigned>(kCXMM);
    }
    case CallingConv::OCAML: {
      return llvm::ArrayRef<unsigned>(kOCamlXMM);
    }
    case CallingConv::EXT: {
      return llvm::ArrayRef<unsigned>(kExtXMM);
    }
  }
}
