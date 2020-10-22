// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

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
static const std::vector<unsigned> kCGPR8 = {
  X86::DIL, X86::SIL, X86::DL,
  X86::CL, X86::R8B, X86::R9B
};
static const std::vector<unsigned> kCGPR16 = {
  X86::DI, X86::SI, X86::DX,
  X86::CX, X86::R8W, X86::R9W
};
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
static const std::vector<unsigned> kCRetGPR8 = {
  X86::AL, X86::DL
};
static const std::vector<unsigned> kCRetGPR16 = {
  X86::AX, X86::DX
};
static const std::vector<unsigned> kCRetGPR32 = {
  X86::EAX, X86::EDX
};
static const std::vector<unsigned> kCRetGPR64 = {
  X86::RAX, X86::RDX
};
static const std::vector<unsigned> kCRetF80 = {
  X86::FP0
};
static const std::vector<unsigned> kCRetXMM = {
  X86::XMM0
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
static const std::vector<unsigned> kOCamlRetGPR8 = {
  X86::AL
};
static const std::vector<unsigned> kOCamlRetGPR16 = {
  X86::AX
};
static const std::vector<unsigned> kOCamlRetGPR32 = {
  X86::EAX
};
static const std::vector<unsigned> kOCamlRetGPR64 = {
  X86::RAX
};
static const std::vector<unsigned> kOCamlRetXMM = {
  X86::XMM0
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to C allocator calls.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlAllocGPR64 = {
  X86::RAX,
};
static const std::vector<unsigned> kOCamlAllocXMM = {
};
static const std::vector<unsigned> kOCamlAllocRetGPR64 = {
  X86::RAX
};

// -----------------------------------------------------------------------------
// Registers used by OCaml GC trampolines.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGcGPR64 = {
};
static const std::vector<unsigned> kOCamlGcXMM = {
};

// -----------------------------------------------------------------------------
static const llvm::TargetRegisterClass *GetRegisterClass(Type type)
{
  switch (type) {
    case Type::I8: return &X86::GR8RegClass;
    case Type::I16: return &X86::GR16RegClass;
    case Type::I32: return &X86::GR32RegClass;
    case Type::I64: return &X86::GR64RegClass;
    case Type::V64: return &X86::GR64RegClass;
    case Type::F32: return &X86::FR32RegClass;
    case Type::F64: return &X86::FR64RegClass;
    case Type::F80: return &X86::RFP80RegClass;
    case Type::I128: llvm_unreachable("invalid argument type");
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86Call::AssignArgC(unsigned i, Type type, ConstRef<Inst> value)
{
  switch (type) {
    case Type::I8:{
      if (argRegs_ < kCGPR8.size()) {
        AssignArgReg(i, type, value, kCGPR8[argRegs_++]);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::I16:{
      if (argRegs_ < kCGPR16.size()) {
        AssignArgReg(i, type, value, kCGPR16[argRegs_++]);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::I32: {
      if (argRegs_ < kCGPR32.size()) {
        AssignArgReg(i, type, value, kCGPR32[argRegs_++]);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::V64:
    case Type::I64: {
      if (argRegs_ < kCGPR64.size()) {
        AssignArgReg(i, type, value, kCGPR64[argRegs_++]);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::F32: case Type::F64: {
      if (argXMMs_ < kCXMM.size()) {
        AssignArgReg(i, type, value, kCXMM[argXMMs_++]);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::F80: {
      AssignArgStack(i, type, value);
      return;
    }
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86Call::AssignArgOCaml(unsigned i, Type type, ConstRef<Inst> value)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
    case Type::V64:
    case Type::I64: {
      if (argRegs_ < kOCamlGPR64.size()) {
        AssignArgReg(i, type, value, kOCamlGPR64[argRegs_++]);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::F32: case Type::F64: {
      if (argXMMs_ < kOCamlXMM.size()) {
        AssignArgReg(i, type, value, kOCamlXMM[argXMMs_++]);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::F80: {
      AssignArgStack(i, type, value);
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86Call::AssignArgOCamlAlloc(unsigned i, Type type, ConstRef<Inst> value)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
    case Type::F32:
    case Type::F64:
    case Type::F80: {
      llvm_unreachable("Invalid argument type");
    }
    case Type::V64:
    case Type::I64: {
      if (argRegs_ < kOCamlAllocGPR64.size()) {
        AssignArgReg(i, type, value, kOCamlAllocGPR64[argRegs_++]);
      } else {
        llvm_unreachable("Too many arguments");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86Call::AssignArgOCamlGc(unsigned i, Type type, ConstRef<Inst> value)
{
  llvm_unreachable("Invalid argument type");
}

// -----------------------------------------------------------------------------
void X86Call::AssignRetC(unsigned i, Type type)
{
  switch (type) {
    case Type::I8: {
      if (retRegs_ < kCRetGPR8.size()) {
        AssignRetReg(i, type, kCRetGPR8[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I16: {
      if (retRegs_ < kCRetGPR16.size()) {
        AssignRetReg(i, type, kCRetGPR16[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I32: {
      if (retRegs_ < kCRetGPR32.size()) {
        AssignRetReg(i, type, kCRetGPR32[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::V64:
    case Type::I64: {
      if (retRegs_ < kCRetGPR64.size()) {
        AssignRetReg(i, type, kCRetGPR64[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F32: case Type::F64: {
      if (retXMMs_ < kCRetXMM.size()) {
        AssignRetReg(i, type, kCRetXMM[retXMMs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F80: {
      if (retFPs_ < kCRetF80.size()) {
        AssignRetReg(i, type, kCRetF80[retFPs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86Call::AssignRetOCaml(unsigned i, Type type)
{
  switch (type) {
    case Type::I8: {
      if (retRegs_ < kOCamlRetGPR8.size()) {
        AssignRetReg(i, type, kOCamlRetGPR8[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I16: {
      if (retRegs_ < kOCamlRetGPR16.size()) {
        AssignRetReg(i, type, kOCamlRetGPR16[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I32: {
      if (retRegs_ < kOCamlRetGPR32.size()) {
        AssignRetReg(i, type, kOCamlRetGPR32[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::V64:
    case Type::I64: {
      if (retRegs_ < kOCamlRetGPR64.size()) {
        AssignRetReg(i, type, kOCamlRetGPR64[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F32: case Type::F64: {
      if (retXMMs_ < kOCamlRetXMM.size()) {
        AssignRetReg(i, type, kOCamlRetXMM[retXMMs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I128:
    case Type::F80: {
      llvm_unreachable("invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86Call::AssignRetOCamlAlloc(unsigned i, Type type)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
    case Type::F32:
    case Type::F64:
    case Type::F80: {
      llvm_unreachable("Invalid argument type");
    }
    case Type::V64:
    case Type::I64: {
      if (retRegs_ < kOCamlAllocGPR64.size()) {
        AssignRetReg(i, type, kOCamlAllocGPR64[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86Call::AssignRetOCamlGc(unsigned i, Type type)
{
  llvm_unreachable("invalid return value");
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetUnusedGPRs() const
{
  return GetGPRs().drop_front(argRegs_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetUsedGPRs() const
{
  return GetGPRs().take_front(argRegs_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetUnusedXMMs() const
{
  return GetXMMs().drop_front(argXMMs_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetUsedXMMs() const
{
  return GetXMMs().take_front(argXMMs_);
}

// -----------------------------------------------------------------------------
void X86Call::AssignArgReg(
    unsigned i,
    Type type,
    ConstRef<Inst> value,
    llvm::Register reg)
{
  args_[i].Index = i;
  args_[i].Kind = ArgLoc::Kind::REG;
  args_[i].Reg = reg;
  args_[i].ArgType = type;
  args_[i].Value = value;
  args_[i].RegClass = GetRegisterClass(type);
  args_[i].VT = GetVT(type);
}

// -----------------------------------------------------------------------------
void X86Call::AssignRetReg(
    unsigned i,
    Type type,
    llvm::Register reg)
{
  rets_[i].Reg = reg;
  rets_[i].VT = GetVT(type);
}

// -----------------------------------------------------------------------------
void X86Call::AssignArgStack(unsigned i, Type type, ConstRef<Inst> value)
{
  size_t size = GetSize(type);

  args_[i].Index = i;
  args_[i].Kind = ArgLoc::Kind::STK;
  args_[i].Idx = stack_;
  args_[i].Size = size;
  args_[i].ArgType = type;
  args_[i].Value = value;
  args_[i].RegClass = GetRegisterClass(type);
  args_[i].VT = GetVT(type);

  stack_ = stack_ + (size + 7) & ~7;
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetGPRs() const
{
  switch (conv_) {
    case CallingConv::C:
    case CallingConv::SETJMP:
    case CallingConv::CAML_RAISE: {
      return llvm::ArrayRef<unsigned>(kCGPR64);
    }
    case CallingConv::CAML: {
      return llvm::ArrayRef<unsigned>(kOCamlGPR64);
    }
    case CallingConv::CAML_ALLOC: {
      return llvm::ArrayRef<unsigned>(kOCamlAllocGPR64);
    }
    case CallingConv::CAML_GC: {
      return llvm::ArrayRef<unsigned>(kOCamlGcGPR64);
    }
  }
  llvm_unreachable("invalid calling convention");
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86Call::GetXMMs() const
{
  switch (conv_) {
    case CallingConv::C:
    case CallingConv::CAML_RAISE:
    case CallingConv::SETJMP: {
      return llvm::ArrayRef<unsigned>(kCXMM);
    }
    case CallingConv::CAML: {
      return llvm::ArrayRef<unsigned>(kOCamlXMM);
    }    case CallingConv::CAML_ALLOC: {
      return llvm::ArrayRef<unsigned>(kOCamlAllocXMM);
    }
    case CallingConv::CAML_GC: {
      return llvm::ArrayRef<unsigned>(kOCamlGcXMM);
    }
  }
  llvm_unreachable("invalid calling convention");
}
