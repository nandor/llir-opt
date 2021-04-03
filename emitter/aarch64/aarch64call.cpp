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
  AArch64::X25, AArch64::X26, AArch64::X27, AArch64::X28,
  AArch64::X0, AArch64::X1, AArch64::X2, AArch64::X3,
  AArch64::X4, AArch64::X5, AArch64::X6, AArch64::X7,
  AArch64::X8, AArch64::X9, AArch64::X10, AArch64::X11,
  AArch64::X12, AArch64::X13, AArch64::X14, AArch64::X15,
};
static const std::vector<unsigned> kOCamlRetGPR32 = {
  AArch64::W25, AArch64::W26, AArch64::W27, AArch64::W28, AArch64::W0
};
static const std::vector<unsigned> kOCamlRetGPR64 = {
  AArch64::X25, AArch64::X26, AArch64::X27, AArch64::X28, AArch64::X0
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to C allocator calls.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlAllocGPR64 = {
  AArch64::X25, AArch64::X26, AArch64::X27, AArch64::X28
};
static const std::vector<unsigned> kOCamlAllocXMM = {
};
static const std::vector<unsigned> kOCamlAllocRetGPR64 = {
  AArch64::X25, AArch64::X26, AArch64::X27, AArch64::X28
};

// -----------------------------------------------------------------------------
// Registers used by OCaml GC trampolines.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGcGPR64 = {
  AArch64::X25, AArch64::X26, AArch64::X27, AArch64::X28
};
static const std::vector<unsigned> kOCamlGcXMM = {
};
static const std::vector<unsigned> kOCamlGcRetGPR64 = {
  AArch64::X25, AArch64::X26, AArch64::X27, AArch64::X28
};

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
void AArch64Call::AssignArgC(unsigned i, FlaggedType type)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      if (argX_ < 8) {
        AssignArgReg(loc, MVT::i32, AArch64::W0 + argX_++);
      } else {
        AssignArgStack(loc, MVT::i32, 4);
      }
      break;
    }
    case Type::V64:
    case Type::I64: {
      if (argX_ < 8) {
        AssignArgReg(loc, MVT::i64, AArch64::X0 + argX_++);
      } else {
        AssignArgStack(loc, MVT::i64, 8);
      }
      break;
    }
    case Type::I128: {
      if (argX_ + 1 < 8) {
        AssignArgReg(loc, MVT::i64, AArch64::X0 + argX_++);
        AssignArgReg(loc, MVT::i64, AArch64::X0 + argX_++);
      } else {
        llvm_unreachable("not implemented");
      }
      break;
    }
    case Type::F32: {
      if (argD_ < 8) {
        AssignArgReg(loc, MVT::f32, AArch64::S0 + argD_++);
      } else {
        AssignArgStack(loc, MVT::f32, 4);
      }
      break;
    }
    case Type::F64: {
      if (argD_ < 8) {
        AssignArgReg(loc, MVT::f64, AArch64::D0 + argD_++);
      } else {
        AssignArgStack(loc, MVT::f64, 8);
      }
      break;
    }
    case Type::F128: {
      if (argD_ < 8) {
        AssignArgReg(loc,  MVT::f128, AArch64::Q0 + argD_++);
      } else {
        AssignArgStack(loc, MVT::f128, 16);
      }
      break;
    }
    case Type::F80: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgOCaml(unsigned i, FlaggedType type)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
    case Type::F80: {
      llvm_unreachable("Invalid argument type");
    }
    case Type::V64:
    case Type::I64: {
      if (argX_ < kOCamlGPR64.size()) {
        AssignArgReg(loc, MVT::i64, kOCamlGPR64[argX_++]);
      } else {
        AssignArgStack(loc, MVT::i64, 8);
      }
      return;
    }
    case Type::F32: {
      if (argD_ < 16) {
        AssignArgReg(loc, MVT::f32, AArch64::S0 + argD_++);
      } else {
        AssignArgStack(loc, MVT::f32, 8);
      }
      return;
    }
    case Type::F64: {
      if (argD_ < 16) {
        AssignArgReg(loc, MVT::f64, AArch64::D0 + argD_++);
      } else {
        AssignArgStack(loc, MVT::f64, 8);
      }
      return;
    }
    case Type::F128: {
      if (argD_ < 8) {
        AssignArgReg(loc, MVT::f128, AArch64::Q0 + argD_++);
      } else {
        AssignArgStack(loc, MVT::f128, 16);
      }
      break;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgOCamlAlloc(unsigned i, FlaggedType type)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type.GetType()) {
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
        AssignArgReg(loc, MVT::i64, kOCamlAllocGPR64[argX_++]);
      } else {
        llvm_unreachable("too many arguments");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgOCamlGc(unsigned i, FlaggedType type)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type.GetType()) {
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
        AssignArgReg(loc, MVT::i64, kOCamlGcGPR64[argX_++]);
      } else {
        llvm_unreachable("Too many arguments");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgWin64(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetC(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      if (retX_ < 8) {
        AssignRetReg(loc, MVT::i32, AArch64::W0 + retX_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::V64:
    case Type::I64: {
      if (retX_ < 8) {
        AssignRetReg(loc, MVT::i64, AArch64::X0 + retX_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::I128: {
      if (retX_ + 1 < 8) {
        AssignRetReg(loc, MVT::i64, AArch64::X0 + retX_++);
        AssignRetReg(loc, MVT::i64, AArch64::X0 + retX_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F32: {
      if (retD_ < 8) {
        AssignRetReg(loc, MVT::f32, AArch64::S0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F64: {
      if (retD_ < 8) {
        AssignRetReg(loc, MVT::f64, AArch64::D0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F128: {
      if (retD_ < 8) {
        AssignRetReg(loc, MVT::f128, AArch64::Q0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F80: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgXen(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetOCaml(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      if (retX_ < kOCamlRetGPR32.size()) {
        AssignRetReg(loc, MVT::i32, kOCamlRetGPR32[retX_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::V64:
    case Type::I64: {
      if (retX_ < kOCamlRetGPR64.size()) {
        AssignRetReg(loc, MVT::i64, kOCamlRetGPR64[retX_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F32: {
      if (retD_ < 1) {
        AssignRetReg(loc, MVT::f32, AArch64::S0 + retD_);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F64: {
      if (retD_ < 1) {
        AssignRetReg(loc, MVT::f64, AArch64::D0 + retD_++);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F128: {
      if (retD_ < 8) {
        AssignRetReg(loc, MVT::f128, AArch64::Q0 + retD_++);
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
void AArch64Call::AssignRetOCamlAlloc(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type.GetType()) {
    case Type::V64:
    case Type::I64: {
      if (retX_ < kOCamlAllocRetGPR64.size()) {
        AssignRetReg(loc, MVT::i64, kOCamlAllocRetGPR64[retX_++]);
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
void AArch64Call::AssignRetOCamlGc(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type.GetType()) {
    case Type::V64:
    case Type::I64: {
      if (retX_ < kOCamlGcRetGPR64.size()) {
        AssignRetReg(loc, MVT::i64, kOCamlGcRetGPR64[retX_++]);
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
void AArch64Call::AssignRetXen(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetWin64(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgReg(ArgLoc &loc, llvm::MVT vt, llvm::Register reg)
{
  loc.Parts.emplace_back(vt, reg);
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignArgStack(ArgLoc &loc, llvm::MVT vt, unsigned size)
{
  loc.Parts.emplace_back(vt, stack_, size);
  stack_ = stack_ + (size + 7) & ~7;
}

// -----------------------------------------------------------------------------
void AArch64Call::AssignRetReg(RetLoc &loc, llvm::MVT vt, llvm::Register reg)
{
  loc.Parts.emplace_back(vt, reg);
}
