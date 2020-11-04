// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Target/AArch64/AArch64ISelLowering.h>
#include <llvm/Target/AArch64/AArch64InstrInfo.h>

#include "emitter/aarch64/aarch64call.h"

namespace AArch64 = llvm::AArch64;
using MVT = llvm::MVT;



// -----------------------------------------------------------------------------
// C calling convention registers
// -----------------------------------------------------------------------------
static const std::vector<llvm::MCPhysReg> kCGPRs = {
  AArch64::X0, AArch64::X1, AArch64::X2, AArch64::X3,
  AArch64::X4, AArch64::X5, AArch64::X6, AArch64::X7,
};

static const std::vector<llvm::MCPhysReg> kCFPRs = {
  AArch64::Q0, AArch64::Q1, AArch64::Q2, AArch64::Q3,
  AArch64::Q4, AArch64::Q5, AArch64::Q6, AArch64::Q7,
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to pass arguments.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGPR64 = {
  AArch64::X25, AArch64::X26,
  AArch64::X0, AArch64::X1, AArch64::X2, AArch64::X3,
  AArch64::X4, AArch64::X5, AArch64::X6, AArch64::X7,
  AArch64::X8, AArch64::X9, AArch64::X10, AArch64::X11,
  AArch64::X12, AArch64::X13, AArch64::X14, AArch64::X15,
};
static const std::vector<unsigned> kOCamlRetGPR32 = {
  AArch64::W25, AArch64::W26, AArch64::W0
};
static const std::vector<unsigned> kOCamlRetGPR64 = {
  AArch64::X25, AArch64::X26, AArch64::X0
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to C allocator calls.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlAllocGPR64 = {
  AArch64::X25, AArch64::X26,
};
static const std::vector<unsigned> kOCamlAllocXMM = {
};
static const std::vector<unsigned> kOCamlAllocRetGPR64 = {
  AArch64::X25, AArch64::X26,
};

// -----------------------------------------------------------------------------
// Registers used by OCaml GC trampolines.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGcGPR64 = {
  AArch64::X25, AArch64::X26,
};
static const std::vector<unsigned> kOCamlGcXMM = {
};
static const std::vector<unsigned> kOCamlGcRetGPR64 = {
  AArch64::X25, AArch64::X26,
};

// -----------------------------------------------------------------------------
static const llvm::TargetRegisterClass *GetRegisterClass(Type type)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:  return &AArch64::GPR32RegClass;
    case Type::V64:  return &AArch64::GPR64RegClass;
    case Type::I64:  return &AArch64::GPR64RegClass;
    case Type::F32:  return &AArch64::FPR32RegClass;
    case Type::F64:  return &AArch64::FPR64RegClass;
    case Type::F128: return &AArch64::FPR128RegClass;
    case Type::I128:
    case Type::F80: {
      llvm_unreachable("invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> AArch64Call::GetUnusedGPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCGPRs).drop_front(argX_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> AArch64Call::GetUsedGPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCGPRs).take_front(argX_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> AArch64Call::GetUnusedFPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCFPRs).drop_front(argD_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> AArch64Call::GetUsedFPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCFPRs).take_front(argD_);
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgC(unsigned i, Type type, ConstRef<Inst> value)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      if (argX_ < 8) {
        AssignArgReg(i, type, MVT::i32, value, AArch64::W0 + argX_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::V64:
    case Type::I64: {
      if (argX_ < 8) {
        AssignArgReg(i, type, MVT::i64, value, AArch64::X0 + argX_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::F32: {
      if (argD_ < 8) {
        AssignArgReg(i, type, MVT::f32, value, AArch64::S0 + argD_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::F64: {
      if (argD_ < 8) {
        AssignArgReg(i, type, MVT::f64, value, AArch64::D0 + argD_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::F128: {
      if (argD_ < 8) {
        AssignArgReg(i, type, MVT::f128, value, AArch64::Q0 + argD_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::F80:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgOCaml(unsigned i, Type type, ConstRef<Inst> value)
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
      if (argX_ < kOCamlGPR64.size()) {
        AssignArgReg(i, type, MVT::i64, value, kOCamlGPR64[argX_++]);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::F32: {
      if (argD_ < 16) {
        AssignArgReg(i, type, MVT::f64, value, AArch64::S0 + argD_++);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::F64: {
      if (argD_ < 16) {
        AssignArgReg(i, type, MVT::f64, value, AArch64::D0 + argD_++);
      } else {
        AssignArgStack(i, type, value);
      }
      return;
    }
    case Type::F128: {
      if (argD_ < 8) {
        AssignArgReg(i, type, MVT::f128, value, AArch64::Q0 + argD_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::F80: {
      AssignArgStack(i, type, value);
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgOCamlAlloc(unsigned i, Type type, ConstRef<Inst> value)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("invalid argument type");
    }
    case Type::V64:
    case Type::I64: {
      if (argX_ < kOCamlAllocGPR64.size()) {
        AssignArgReg(i, type, MVT::i64, value, kOCamlAllocGPR64[argX_++]);
      } else {
        llvm_unreachable("too many arguments");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgOCamlGc(unsigned i, Type type, ConstRef<Inst> value)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("Invalid argument type");
    }
    case Type::V64:
    case Type::I64: {
      if (argX_ < kOCamlGcGPR64.size()) {
        AssignArgReg(i, type, MVT::i64, value, kOCamlGcGPR64[argX_++]);
      } else {
        llvm_unreachable("Too many arguments");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetC(unsigned i, Type type)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      if (retX_ < 8) {
        AssignRetReg(i, type, MVT::i32, AArch64::W0 + retX_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::V64:
    case Type::I64: {
      if (retX_ < 8) {
        AssignRetReg(i, type, MVT::i64, AArch64::X0 + retX_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F32: {
      if (retD_ < 8) {
        AssignRetReg(i, type, MVT::f32, AArch64::S0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F64: {
      if (retD_ < 8) {
        AssignRetReg(i, type, MVT::f64, AArch64::D0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F128: {
      if (retD_ < 8) {
        AssignRetReg(i, type, MVT::f128, AArch64::Q0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F80:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetOCaml(unsigned i, Type type)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      if (retX_ < kOCamlRetGPR32.size()) {
        AssignRetReg(i, type, MVT::i32, kOCamlRetGPR32[retX_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::V64:
    case Type::I64: {
      if (retX_ < kOCamlRetGPR64.size()) {
        AssignRetReg(i, type, MVT::i64, kOCamlRetGPR64[retX_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F32: {
      if (retD_ < 1) {
        AssignRetReg(i, type, MVT::f32, AArch64::S0 + retD_);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F64: {
      if (retD_ < 1) {
        AssignRetReg(i, type, MVT::f64, AArch64::D0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F128: {
      if (retD_ < 8) {
        AssignRetReg(i, type, MVT::f128, AArch64::Q0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::I128:
    case Type::F80: {
      llvm_unreachable("invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetOCamlAlloc(unsigned i, Type type)
{
  switch (type) {
    case Type::V64:
    case Type::I64: {
      if (retX_ < kOCamlAllocRetGPR64.size()) {
        AssignRetReg(i, type, MVT::i64, kOCamlAllocRetGPR64[retX_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::F32:
    case Type::F64:
    case Type::I128:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("invalid return type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetOCamlGc(unsigned i, Type type)
{
  switch (type) {
    case Type::V64:
    case Type::I64: {
      if (retX_ < kOCamlGcRetGPR64.size()) {
        AssignRetReg(i, type, MVT::i32, kOCamlGcRetGPR64[retX_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::F32:
    case Type::F64:
    case Type::I128:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("invalid return type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgReg(
    unsigned i,
    Type type,
    llvm::MVT vt,
    ConstRef<Inst> value,
    llvm::Register reg)
{
  args_[i].Index = i;
  args_[i].Kind = ArgLoc::Kind::REG;
  args_[i].Reg = reg;
  args_[i].ArgType = type;
  args_[i].Value = value;
  args_[i].RegClass = GetRegisterClass(type);
  args_[i].VT = vt;
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetReg(
    unsigned i,
    Type type,
    llvm::MVT vt,
    llvm::Register reg)
{
  rets_[i].Reg = reg;
  rets_[i].VT = vt;
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgStack(unsigned i, Type type, ConstRef<Inst> value)
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
