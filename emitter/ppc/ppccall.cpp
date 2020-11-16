// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Target/PowerPC/PPCISelLowering.h>
#include <llvm/Target/PowerPC/PPCInstrInfo.h>

#include "emitter/ppc/ppccall.h"

namespace PPC = llvm::PPC;
using MVT = llvm::MVT;



// -----------------------------------------------------------------------------
// C calling convention registers
// -----------------------------------------------------------------------------
static const std::vector<llvm::MCPhysReg> kCGPR = {
  PPC::X3, PPC::X4, PPC::X5, PPC::X6, PPC::X7, PPC::X8, PPC::X9, PPC::X10
};
static const std::vector<llvm::MCPhysReg> kCFPR = {
  PPC::F1, PPC::F2, PPC::F3, PPC::F4, PPC::F5, PPC::F6, PPC::F7, PPC::F8,
  PPC::F9, PPC::F10, PPC::F11, PPC::F12, PPC::F13
};
static const std::vector<llvm::MCPhysReg> kCRetGPR = {
  PPC::X3, PPC::X4, PPC::X5, PPC::X6
};
static const std::vector<llvm::MCPhysReg> kCRetFPR = {
   PPC::F1, PPC::F2, PPC::F3, PPC::F4,
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to pass arguments.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGPR = {
  PPC::X28, PPC::X29, PPC::X30, PPC::X31,
  PPC::X3, PPC::X4, PPC::X5, PPC::X6, PPC::X7, PPC::X8, PPC::X9, PPC::X10
};
static const std::vector<unsigned> kOCamlFPR = {
  PPC::F1, PPC::F2, PPC::F3, PPC::F4, PPC::F5, PPC::F6, PPC::F7, PPC::F8,
  PPC::F9, PPC::F10, PPC::F11, PPC::F12, PPC::F13
};
static const std::vector<unsigned> kOCamlRetGPR = {
  PPC::X28, PPC::X29, PPC::X30, PPC::X31,
  PPC::X3, PPC::X4, PPC::X5, PPC::X6
};
static const std::vector<unsigned> kOCamlRetFPR = {
   PPC::F1, PPC::F2, PPC::F3, PPC::F4,
};

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> PPCCall::GetUnusedGPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCGPR).drop_front(argG_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> PPCCall::GetUsedGPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCGPR).take_front(argG_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> PPCCall::GetUnusedFPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCFPR).drop_front(argF_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> PPCCall::GetUsedFPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCFPR).take_front(argF_);
}

// -----------------------------------------------------------------------------
PPCCall::PPCCall(const Func *func)
  : CallLowering(func)
{
  AnalyseFunc(func);
}

// -----------------------------------------------------------------------------
PPCCall::PPCCall(const CallSite *inst)
  : CallLowering(inst)
{
  AnalyseCall(inst);
}

// -----------------------------------------------------------------------------
PPCCall::PPCCall(const ReturnInst *inst)
  : CallLowering(inst)
{
  AnalyseReturn(inst);
}

// -----------------------------------------------------------------------------
PPCCall::PPCCall(const RaiseInst *inst)
  : CallLowering(inst)
{
  AnalyseRaise(inst);
}

// -----------------------------------------------------------------------------
void PPCCall::AssignArgC(unsigned i, Type type, ConstRef<Inst> value)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (argG_ < kCGPR.size()) {
        AssignArgReg(i, MVT::i64, kCGPR[argG_++]);
      } else {
        AssignArgStack(i, MVT::i64, 8);
      }
      stack_ += 8;
      return;
    }
    case Type::F32: {
      if (argF_ < kCFPR.size()) {
        AssignArgReg(i, MVT::f32, kCFPR[argF_++]);
        argG_++;
      } else {
        AssignArgStack(i, MVT::f32, 4);
      }
      stack_ += 8;
      return;
    }
    case Type::F64: {
      if (argF_ < kCFPR.size()) {
        AssignArgReg(i, MVT::f64, kCFPR[argF_++]);
        argG_++;
      } else {
        AssignArgStack(i, MVT::f64, 8);
      }
      stack_ += 8;
      return;
    }
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void PPCCall::AssignArgOCaml(unsigned i, Type type, ConstRef<Inst> value)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (argG_ < kOCamlGPR.size()) {
        AssignArgReg(i, MVT::i64, kOCamlGPR[argG_++]);
      } else {
        AssignArgStack(i, MVT::i64, 8);
      }
      stack_ += 8;
      return;
    }
    case Type::F32: {
      if (argF_ < kOCamlFPR.size()) {
        AssignArgReg(i, MVT::f32, kOCamlFPR[argF_++]);
      } else {
        AssignArgStack(i, MVT::f32, 4);
      }
      stack_ += 8;
      return;
    }
    case Type::F64: {
      if (argF_ < kOCamlFPR.size()) {
        AssignArgReg(i, MVT::f64, kOCamlFPR[argF_++]);
      } else {
        AssignArgStack(i, MVT::f64, 8);
      }
      stack_ += 8;
      return;
    }
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void PPCCall::AssignRetC(unsigned i, Type type)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (retG_ < kCRetGPR.size()) {
        AssignRetReg(i, MVT::i64, kCRetGPR[retG_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F32: {
      if (retF_ < kCRetFPR.size()) {
        AssignRetReg(i, MVT::f32, kCRetFPR[retF_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F64: {
      if (retF_ < kCRetFPR.size()) {
        AssignRetReg(i, MVT::f64, kCRetFPR[retF_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void PPCCall::AssignRetOCaml(unsigned i, Type type)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I64: {
      if (retG_ < kOCamlRetGPR.size()) {
        AssignRetReg(i, MVT::i64, kOCamlRetGPR[retG_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F32: {
      if (retF_ < kOCamlRetFPR.size()) {
        AssignRetReg(i, MVT::f32, kOCamlRetFPR[retF_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F64: {
      if (retF_ < kOCamlRetFPR.size()) {
        AssignRetReg(i, MVT::f64, kOCamlRetFPR[retF_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F80:
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void PPCCall::AssignArgReg(unsigned i, llvm::MVT vt, llvm::Register reg)
{
  args_[i].Parts.emplace_back(vt, reg);
}

// -----------------------------------------------------------------------------
void PPCCall::AssignRetReg(unsigned i, llvm::MVT vt, llvm::Register reg)
{
  rets_[i].Parts.emplace_back(vt, reg);
}

// -----------------------------------------------------------------------------
void PPCCall::AssignArgStack(unsigned i, llvm::MVT vt, unsigned size)
{
  args_[i].Parts.emplace_back(vt, stack_, size);
  hasStackArgs_ = true;
}
