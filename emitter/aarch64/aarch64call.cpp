// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Target/AArch64/AArch64ISelLowering.h>
#include <llvm/Target/AArch64/AArch64InstrInfo.h>

#include "emitter/aarch64/aarch64call.h"

namespace AArch64 = llvm::AArch64;
using MVT = llvm::MVT;



// -----------------------------------------------------------------------------
static const llvm::TargetRegisterClass *GetRegisterClass(Type type)
{
  switch (type) {
    case Type::I32: return &AArch64::GPR32RegClass;
    case Type::I64: return &AArch64::GPR64RegClass;
    case Type::F32: return &AArch64::FPR32RegClass;
    case Type::F64: return &AArch64::FPR64RegClass;
    case Type::I8:
    case Type::I16:
    case Type::I128:
    case Type::F80:
      llvm_unreachable("invalid argument type");
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgC(unsigned i, Type type, const Inst *value)
{
  switch (type) {
    case Type::I32: {
      if (argX_ < 8) {
        AssignArgReg(i, type, value, AArch64::W0 + argX_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::I64: {
      if (argX_ < 8) {
        AssignArgReg(i, type, value, AArch64::X0 + argX_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::F32: {
      if (argD_ < 8) {
        AssignArgReg(i, type, value, AArch64::S0 + argD_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::F64: {
      if (argD_ < 8) {
        AssignArgReg(i, type, value, AArch64::D0 + argD_++);
      } else {
        AssignArgStack(i, type, value);
      }
      break;
    }
    case Type::I8:
    case Type::I16:
    case Type::F80:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgOCaml(unsigned i, Type type, const Inst *value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgOCamlAlloc(unsigned i, Type type, const Inst *value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgOCamlGc(unsigned i, Type type, const Inst *value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetC(unsigned i, Type type)
{
  switch (type) {
    case Type::I32: {
      if (retX_ < 8) {
        AssignRetReg(i, type, AArch64::W0 + retX_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::I64: {
      if (retX_ < 8) {
        AssignRetReg(i, type, AArch64::X0 + retX_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F32: {
      if (retD_ < 8) {
        AssignRetReg(i, type, AArch64::S0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F64: {
      if (retD_ < 8) {
        AssignRetReg(i, type, AArch64::D0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::I8:
    case Type::I16:
    case Type::F80:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetOCaml(unsigned i, Type type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetOCamlAlloc(unsigned i, Type type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetOCamlGc(unsigned i, Type type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgReg(
    unsigned i,
    Type type,
    const Inst *value,
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
void AArch64Call::AssignRetReg(
    unsigned i,
    Type type,
    llvm::Register reg)
{
  rets_[i].Reg = reg;
  rets_[i].VT = GetVT(type);
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgStack(unsigned i, Type type, const Inst *value)
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
