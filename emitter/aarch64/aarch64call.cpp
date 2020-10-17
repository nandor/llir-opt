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
void AArch64Call::AssignC(unsigned i, Type type, const Inst *value)
{
  switch (type) {
    case Type::I32: {
      if (x_ < 8) {
        AssignReg(i, type, value, AArch64::W0 + x_++);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
    case Type::I64: {
      if (x_ < 8) {
        AssignReg(i, type, value, AArch64::X0 + x_++);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
    case Type::F32: {
      if (d_ < 8) {
        AssignReg(i, type, value, AArch64::S0 + d_++);
      } else {
        AssignStack(i, type, value);
      }
      break;
    }
    case Type::F64: {
      if (d_ < 8) {
        AssignReg(i, type, value, AArch64::D0 + d_++);
      } else {
        AssignStack(i, type, value);
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
void AArch64Call::AssignOCaml(unsigned i, Type type, const Inst *value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignOCamlAlloc(unsigned i, Type type, const Inst *value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignOCamlGc(unsigned i, Type type, const Inst *value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
CallLowering::RetLoc AArch64Call::Return(Type type) const
{
  switch (type) {
    case Type::I32: return { AArch64::W0, MVT::i32 };
    case Type::I64: return { AArch64::X0, MVT::i64 };
    case Type::F32: return { AArch64::S0, MVT::f32 };
    case Type::F64: return { AArch64::D0, MVT::f64 };
    case Type::I8:
    case Type::I16:
    case Type::F80:
    case Type::I128: {
      llvm_unreachable("invalid return type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignReg(
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
void AArch64Call::AssignStack(unsigned i, Type type, const Inst *value)
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
