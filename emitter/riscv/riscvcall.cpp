// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Target/RISCV/RISCVISelLowering.h>
#include <llvm/Target/RISCV/RISCVInstrInfo.h>

#include "emitter/riscv/riscvcall.h"

namespace RISCV = llvm::RISCV;
using MVT = llvm::MVT;



// -----------------------------------------------------------------------------
// C calling convention registers
// -----------------------------------------------------------------------------
static const std::vector<llvm::MCPhysReg> kCGPRs = {
  RISCV::X10, RISCV::X11, RISCV::X12, RISCV::X13,
  RISCV::X14, RISCV::X15, RISCV::X16, RISCV::X17,
};
static const std::vector<llvm::MCPhysReg> kCRetGPRs = {
  RISCV::X10, RISCV::X11,
};
static const std::vector<llvm::MCPhysReg> kCFPR32s = {
  RISCV::F10_F, RISCV::F11_F, RISCV::F12_F, RISCV::F13_F,
  RISCV::F14_F, RISCV::F15_F, RISCV::F16_F, RISCV::F17_F,
};
static const std::vector<llvm::MCPhysReg> kCRetFPR32s = {
  RISCV::F10_F, RISCV::F11_F,
};
static const std::vector<llvm::MCPhysReg> kCFPR64s = {
  RISCV::F10_D, RISCV::F11_D, RISCV::F12_D, RISCV::F13_D,
  RISCV::F14_D, RISCV::F15_D, RISCV::F16_D, RISCV::F17_D,
};
static const std::vector<llvm::MCPhysReg> kCRetFPR64s = {
  RISCV::F10_D, RISCV::F11_D,
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to pass arguments.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGPRs = {
  RISCV::X8, RISCV::X9, RISCV::X26, RISCV::X27,
  RISCV::X10, RISCV::X11, RISCV::X12, RISCV::X13,
  RISCV::X14, RISCV::X15, RISCV::X16, RISCV::X17,
  RISCV::X18, RISCV::X19, RISCV::X20, RISCV::X21,
  RISCV::X22, RISCV::X23, RISCV::X24, RISCV::X25,
};
static const std::vector<unsigned> kOCamlRetGPRs = {
  RISCV::X8, RISCV::X9, RISCV::X26, RISCV::X27,
  RISCV::X10,
};
static const std::vector<unsigned> kOCamlFPR32s = {
  RISCV::F10_F, RISCV::F11_F, RISCV::F12_F, RISCV::F13_F,
  RISCV::F14_F, RISCV::F15_F, RISCV::F16_F, RISCV::F17_F,
  RISCV::F18_F, RISCV::F19_F, RISCV::F20_F, RISCV::F21_F,
  RISCV::F22_F, RISCV::F23_F, RISCV::F24_F, RISCV::F25_F,
};
static const std::vector<unsigned> kOCamlRetFPR32s = {
  RISCV::F10_F,
};
static const std::vector<unsigned> kOCamlFPR64s = {
  RISCV::F10_D, RISCV::F11_D, RISCV::F12_D, RISCV::F13_D,
  RISCV::F14_D, RISCV::F15_D, RISCV::F16_D, RISCV::F17_D,
  RISCV::F18_D, RISCV::F19_D, RISCV::F20_D, RISCV::F21_D,
  RISCV::F22_D, RISCV::F23_D, RISCV::F24_D, RISCV::F25_D,
};
static const std::vector<unsigned> kOCamlRetFPR64s = {
  RISCV::F10_D,
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to C allocator calls.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlAllocGPRs = {
  RISCV::X8, RISCV::X9, RISCV::X26, RISCV::X27,
};
static const std::vector<unsigned> kOCamlAllocRetGPRs = {
  RISCV::X8, RISCV::X9, RISCV::X26, RISCV::X27,
};

// -----------------------------------------------------------------------------
// Registers used by OCaml GC trampolines.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGcGPRs = {
  RISCV::X8, RISCV::X9, RISCV::X26, RISCV::X27,
};
static const std::vector<unsigned> kOCamlGcRetGPRs = {
  RISCV::X8, RISCV::X9, RISCV::X26, RISCV::X27,
};


// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> RISCVCall::GetUnusedGPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCGPRs).drop_front(argI_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> RISCVCall::GetUsedGPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCGPRs).take_front(argI_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> RISCVCall::GetUnusedFPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCFPR64s).drop_front(argF_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> RISCVCall::GetUsedFPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCFPR64s).take_front(argF_);
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgC(unsigned i, Type type, ConstRef<Inst> value)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (argI_ < kCGPRs.size()) {
        AssignArgReg(loc, MVT::i64, kCGPRs[argI_++]);
      } else {
        AssignArgStack(loc, MVT::i64, 8);
      }
      break;
    }
    case Type::F32: {
      if (i < numFixedArgs_) {
        if (argF_ < kCFPR32s.size()) {
          AssignArgReg(loc, MVT::f32, kCFPR32s[argF_++]);
        } else {
          AssignArgStack(loc, MVT::f32, 8);
        }
      } else {
        if (argI_ < kCGPRs.size()) {
          AssignArgReg(loc, MVT::i32, kCGPRs[argI_++]);
        } else {
          AssignArgStack(loc, MVT::f32, 8);
        }
      }
      break;
    }
    case Type::F64: {
      if (i < numFixedArgs_) {
        if (argF_ < kCFPR64s.size()) {
          AssignArgReg(loc, MVT::f64, kCFPR64s[argF_++]);
        } else {
          AssignArgStack(loc, MVT::f64, 8);
        }
      } else {
        if (argI_ < kCGPRs.size()) {
          AssignArgReg(loc, MVT::i64, kCGPRs[argI_++]);
        } else {
          AssignArgStack(loc, MVT::f64, 8);
        }
      }
      break;
    }
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgOCaml(unsigned i, Type type, ConstRef<Inst> value)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (argI_ < kOCamlGPRs.size()) {
        AssignArgReg(loc, MVT::i64, kOCamlGPRs[argI_++]);
      } else {
        AssignArgStack(loc, MVT::i64, 8);
      }
      break;
    }
    case Type::F32: {
      if (argF_ < kOCamlGPRs.size()) {
        AssignArgReg(loc, MVT::f32, kOCamlGPRs[argF_++]);
      } else {
        AssignArgStack(loc, MVT::f32, 4);
      }
      break;
    }
    case Type::F64: {
      if (argF_ < kOCamlFPR64s.size()) {
        AssignArgReg(loc, MVT::f64, kOCamlFPR64s[argF_++]);
      } else {
        AssignArgStack(loc, MVT::f64, 8);
      }
      break;
    }
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgOCamlAlloc(unsigned i, Type type, ConstRef<Inst> value)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type) {
    case Type::V64:
    case Type::I64: {
      if (argI_ < kOCamlAllocGPRs.size()) {
        AssignArgReg(loc, MVT::i64, kOCamlAllocGPRs[argI_++]);
      } else {
        AssignArgStack(loc, MVT::i64, 8);
      }
      break;
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgOCamlGc(unsigned i, Type type, ConstRef<Inst> value)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type) {
    case Type::V64:
    case Type::I64: {
      if (argI_ < kOCamlGcGPRs.size()) {
        AssignArgReg(loc, MVT::i64, kOCamlGcGPRs[argI_++]);
      } else {
        AssignArgStack(loc, MVT::i64, 8);
      }
      break;
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetC(unsigned i, Type type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (retI_ < kCRetGPRs.size()) {
        AssignRetReg(loc, MVT::i64, kCRetGPRs[retI_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F32: {
      if (retF_ < kCRetFPR32s.size()) {
        AssignRetReg(loc, MVT::f32, kCRetFPR32s[retF_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F64: {
      if (retF_ < kCRetFPR64s.size()) {
        AssignRetReg(loc, MVT::f64, kCRetFPR64s[retF_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetOCaml(unsigned i, Type type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (retI_ < kOCamlRetGPRs.size()) {
        AssignRetReg(loc, MVT::i64, kOCamlRetGPRs[retI_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F32: {
      if (retF_ < kOCamlRetFPR32s.size()) {
        AssignRetReg(loc, MVT::f32, kOCamlRetFPR32s[retF_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F64: {
      if (retF_ < kOCamlRetFPR64s.size()) {
        AssignRetReg(loc, MVT::f64, kOCamlRetFPR64s[retF_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetOCamlAlloc(unsigned i, Type type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (retI_ < kOCamlAllocRetGPRs.size()) {
        AssignRetReg(loc, MVT::i64, kOCamlAllocRetGPRs[retI_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetOCamlGc(unsigned i, Type type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (retI_ < kOCamlGcRetGPRs.size()) {
        AssignRetReg(loc, MVT::i64, kOCamlGcRetGPRs[retI_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      break;
    }
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgReg(ArgLoc &loc, llvm::MVT vt, llvm::Register reg)
{
  loc.Parts.emplace_back(vt, reg);
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgStack(ArgLoc &loc, llvm::MVT vt, unsigned size)
{
  loc.Parts.emplace_back(vt, stack_, size);
  stack_ = stack_ + (size + 7) & ~7;
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetReg(RetLoc &loc, llvm::MVT vt, llvm::Register reg)
{
  loc.Parts.emplace_back(vt, reg);
}
