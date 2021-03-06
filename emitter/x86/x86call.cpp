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
static const std::vector<unsigned> k32CRetGPR8 = {
  X86::AL, X86::DL
};
static const std::vector<unsigned> k32CRetGPR16 = {
  X86::AX, X86::DX
};
static const std::vector<unsigned> k32CRetGPR32 = {
  X86::EAX, X86::EDX
};
static const std::vector<unsigned> k32CRetFP = {
  X86::FP0, X86::FP1
};

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
static const std::vector<unsigned> k64CRetGPR8 = {
  X86::AL, X86::DL
};
static const std::vector<unsigned> k64CRetGPR16 = {
  X86::AX, X86::DX
};
static const std::vector<unsigned> k64CRetGPR32 = {
  X86::EAX, X86::EDX
};
static const std::vector<unsigned> k64CRetGPR64 = {
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
  X86::R14, X86::R15,
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
  X86::R14B, X86::R15, X86::AL
};
static const std::vector<unsigned> kOCamlRetGPR16 = {
  X86::R14W, X86::R15, X86::AX
};
static const std::vector<unsigned> kOCamlRetGPR32 = {
  X86::R14D, X86::R15, X86::EAX
};
static const std::vector<unsigned> kOCamlRetGPR64 = {
  X86::R14, X86::R15, X86::RAX
};
static const std::vector<unsigned> kOCamlRetXMM = {
  X86::XMM0
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to C allocator calls.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlAllocGPR64 = {
  X86::R14, X86::R15
};
static const std::vector<unsigned> kOCamlAllocXMM = {
};
static const std::vector<unsigned> kOCamlAllocRetGPR64 = {
  X86::R14, X86::R15
};

// -----------------------------------------------------------------------------
// Registers used by OCaml GC trampolines.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGcGPR64 = {
  X86::R14, X86::R15,
};
static const std::vector<unsigned> kOCamlGcXMM = {
};
static const std::vector<unsigned> kOCamlGcRetGPR64 = {
  X86::R14, X86::R15
};

// -----------------------------------------------------------------------------
// Registers used by Xen hypercalls.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kXenGPR64 = {
  X86::RDI, X86::RSI, X86::RDX, X86::R10, X86::R8, X86::R9
};
static const std::vector<unsigned> kXenRetGPR64 = {
  X86::RAX
};

// -----------------------------------------------------------------------------
// Registers used by multiboot.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kMultiboot = {
  X86::EAX, X86::EBX
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
    case Type::F128:
    case Type::I128: llvm_unreachable("invalid argument type");
  }
  llvm_unreachable("invalid type");
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
void X86Call::AssignArgReg(ArgLoc &loc, llvm::MVT vt, llvm::Register reg)
{
  loc.Parts.emplace_back(vt, reg);
}

// -----------------------------------------------------------------------------
void X86Call::AssignArgStack(ArgLoc &loc, llvm::MVT vt, unsigned size)
{
  stack_ = llvm::alignTo(stack_, llvm::Align(8));
  loc.Parts.emplace_back(vt, stack_, size);
  stack_ = stack_ + size;
}

// -----------------------------------------------------------------------------
void X86Call::AssignArgByVal(
    ArgLoc &loc,
    llvm::MVT vt,
    unsigned size,
    llvm::Align align)
{
  stack_ = llvm::alignTo(stack_, align);
  loc.Parts.emplace_back(vt, stack_, size, align);
  stack_ = stack_ + size;
  maxAlign_ = std::max(maxAlign_, align);
}

// -----------------------------------------------------------------------------
void X86Call::AssignRetReg(RetLoc &loc, llvm::MVT vt, llvm::Register reg)
{
  loc.Parts.emplace_back(vt, reg);
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignArgC(unsigned i, FlaggedType type)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  Type ty = type.GetType();
  AssignArgStack(loc, GetVT(ty), GetSize(ty));
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignArgOCaml(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignArgOCamlAlloc(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignArgOCamlGc(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignArgXen(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignArgMultiboot(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignArgWin64(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignRetC(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (auto ty = type.GetType()) {
    case Type::I8: {
      if (retRegs_ < k32CRetGPR8.size()) {
        AssignRetReg(loc, MVT::i8, k32CRetGPR8[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I16: {
      if (retRegs_ < k32CRetGPR16.size()) {
        AssignRetReg(loc, MVT::i16, k32CRetGPR16[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I32: {
      if (retRegs_ < k32CRetGPR32.size()) {
        AssignRetReg(loc, MVT::i32, k32CRetGPR32[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::V64:
    case Type::I64: {
      if (retRegs_ + 1 < k32CRetGPR32.size()) {
        AssignRetReg(loc, MVT::i32, k32CRetGPR32[retRegs_++]);
        AssignRetReg(loc, MVT::i32, k32CRetGPR32[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F32:
    case Type::F64:
    case Type::F80: {
      if (retRegs_ < k32CRetGPR32.size()) {
        AssignRetReg(loc, GetVT(ty), k32CRetFP[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I128:
    case Type::F128: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignRetOCaml(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignRetOCamlAlloc(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignRetOCamlGc(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignRetXen(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void X86_32Call::AssignRetWin64(unsigned i, FlaggedType type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86_32Call::GetGPRs() const
{
  return {};
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86_32Call::GetXMMs() const
{
  return {};
}


// -----------------------------------------------------------------------------
void X86_64Call::AssignArgC(unsigned i, FlaggedType type)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type.GetType()) {
    case Type::I8:{
      if (argRegs_ < kCGPR8.size()) {
        AssignArgReg(loc, MVT::i8, kCGPR8[argRegs_++]);
      } else {
        AssignArgStack(loc, MVT::i8, 1);
      }
      return;
    }
    case Type::I16:{
      if (argRegs_ < kCGPR16.size()) {
        AssignArgReg(loc, MVT::i16, kCGPR16[argRegs_++]);
      } else {
        AssignArgStack(loc, MVT::i16, 2);
      }
      return;
    }
    case Type::I32: {
      if (argRegs_ < kCGPR32.size()) {
        AssignArgReg(loc, MVT::i32, kCGPR32[argRegs_++]);
      } else {
        AssignArgStack(loc, MVT::i32, 4);
      }
      return;
    }
    case Type::V64:
    case Type::I64: {
      const auto &f = type.GetFlag();
      if (f.IsByVal()) {
        AssignArgByVal(loc, MVT::i64, f.GetByValSize(), f.GetByValAlign());
      } else {
        if (argRegs_ < kCGPR64.size()) {
          AssignArgReg(loc, MVT::i64, kCGPR64[argRegs_++]);
        } else {
          AssignArgStack(loc, MVT::i64, 8);
        }
      }
      return;
    }
    case Type::F32: {
      if (argXMMs_ < kCXMM.size()) {
        AssignArgReg(loc, MVT::f32, kCXMM[argXMMs_++]);
      } else {
        AssignArgStack(loc, MVT::f32, 4);
      }
      return;
    }
    case Type::F64: {
      if (argXMMs_ < kCXMM.size()) {
        AssignArgReg(loc, MVT::f64, kCXMM[argXMMs_++]);
      } else {
        AssignArgStack(loc, MVT::f64, 8);
      }
      return;
    }
    case Type::F80: {
      AssignArgStack(loc, MVT::f80, 10);
      return;
    }
    case Type::F128:
    case Type::I128: {
      llvm_unreachable("Invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignArgOCaml(unsigned i, FlaggedType type)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
    case Type::F128: {
      llvm_unreachable("Invalid argument type");
    }
    case Type::V64:
    case Type::I64: {
      if (argRegs_ < kOCamlGPR64.size()) {
        AssignArgReg(loc, MVT::i64, kOCamlGPR64[argRegs_++]);
      } else {
        AssignArgStack(loc, MVT::i64, 8);
      }
      return;
    }
    case Type::F32: {
      if (argXMMs_ < kOCamlXMM.size()) {
        AssignArgReg(loc, MVT::f32, kOCamlXMM[argXMMs_++]);
      } else {
        AssignArgStack(loc, MVT::f32, 4);
      }
      return;
    }
    case Type::F64: {
      if (argXMMs_ < kOCamlXMM.size()) {
        AssignArgReg(loc, MVT::f64, kOCamlXMM[argXMMs_++]);
      } else {
        AssignArgStack(loc, MVT::f64, 8);
      }
      return;
    }
    case Type::F80: {
      AssignArgStack(loc, MVT::f80, 10);
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignArgOCamlAlloc(unsigned i, FlaggedType type)
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
      if (argRegs_ < kOCamlAllocGPR64.size()) {
        AssignArgReg(loc, MVT::i64, kOCamlAllocGPR64[argRegs_++]);
      } else {
        llvm_unreachable("Too many arguments");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignArgOCamlGc(unsigned i, FlaggedType type)
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
      if (argRegs_ < kOCamlGcGPR64.size()) {
        AssignArgReg(loc, MVT::i64, kOCamlGcGPR64[argRegs_++]);
      } else {
        llvm_unreachable("Too many arguments");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignArgXen(unsigned i, FlaggedType type)
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
    case Type::I64:
    case Type::V64: {
      if (argRegs_ < kXenGPR64.size()) {
        AssignArgReg(loc, MVT::i64, kXenGPR64[argRegs_++]);
      } else {
        llvm_unreachable("Too many arguments");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignArgWin64(unsigned i, FlaggedType type)
{
  // TODO: implement proper calling convention.
  // Win64 uses rcx, rdx, r8, r9
  return AssignArgC(i, type);
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignArgMultiboot(unsigned i, FlaggedType type)
{
  ArgLoc &loc = args_.emplace_back(i, type);
  switch (type.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I64:
    case Type::V64:
    case Type::I128:
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("Invalid argument type");
    }
    case Type::I32: {
      if (argRegs_ < kMultiboot.size()) {
        AssignArgReg(loc, MVT::i32, kMultiboot[argRegs_++]);
      } else {
        llvm_unreachable("Too many arguments");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignRetC(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type.GetType()) {
    case Type::I8: {
      if (retRegs_ < k64CRetGPR8.size()) {
        AssignRetReg(loc, MVT::i8, k64CRetGPR8[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I16: {
      if (retRegs_ < k64CRetGPR16.size()) {
        AssignRetReg(loc, MVT::i16, k64CRetGPR16[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I32: {
      if (retRegs_ < k64CRetGPR32.size()) {
        AssignRetReg(loc, MVT::i32, k64CRetGPR32[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::V64:
    case Type::I64: {
      if (retRegs_ < k64CRetGPR64.size()) {
        AssignRetReg(loc, MVT::i64, k64CRetGPR64[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F32: {
      if (retXMMs_ < kCRetXMM.size()) {
        AssignRetReg(loc, MVT::f32, kCRetXMM[retXMMs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F64: {
      if (retXMMs_ < kCRetXMM.size()) {
        AssignRetReg(loc, MVT::f64, kCRetXMM[retXMMs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F80: {
      if (retFPs_ < kCRetF80.size()) {
        AssignRetReg(loc, MVT::f80, kCRetF80[retFPs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I128:
    case Type::F128: {
      llvm_unreachable("Invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignRetOCaml(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
  switch (type.GetType()) {
    case Type::I8: {
      if (retRegs_ < kOCamlRetGPR8.size()) {
        AssignRetReg(loc, MVT::i8, kOCamlRetGPR8[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I16: {
      if (retRegs_ < kOCamlRetGPR16.size()) {
        AssignRetReg(loc, MVT::i16, kOCamlRetGPR16[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I32: {
      if (retRegs_ < kOCamlRetGPR32.size()) {
        AssignRetReg(loc, MVT::i32, kOCamlRetGPR32[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::V64:
    case Type::I64: {
      if (retRegs_ < kOCamlRetGPR64.size()) {
        AssignRetReg(loc, MVT::i64, kOCamlRetGPR64[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F32: {
      if (retXMMs_ < kOCamlRetXMM.size()) {
        AssignRetReg(loc, MVT::f32, kOCamlRetXMM[retXMMs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::F64: {
      if (retXMMs_ < kOCamlRetXMM.size()) {
        AssignRetReg(loc, MVT::f64, kOCamlRetXMM[retXMMs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
    case Type::I128:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("invalid argument type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignRetOCamlAlloc(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
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
      if (retRegs_ < kOCamlAllocRetGPR64.size()) {
        AssignRetReg(loc, MVT::i64, kOCamlAllocRetGPR64[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignRetOCamlGc(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
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
      if (retRegs_ < kOCamlGcRetGPR64.size()) {
        AssignRetReg(loc, MVT::i64, kOCamlGcRetGPR64[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignRetXen(unsigned i, FlaggedType type)
{
  RetLoc &loc = rets_.emplace_back(i);
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
      if (retRegs_ < kXenRetGPR64.size()) {
        AssignRetReg(loc, MVT::i64, kXenRetGPR64[retRegs_++]);
      } else {
        llvm_unreachable("cannot return value");
      }
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void X86_64Call::AssignRetWin64(unsigned i, FlaggedType type)
{
  return AssignRetC(i, type);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86_64Call::GetGPRs() const
{
  switch (conv_) {
    case CallingConv::C:
    case CallingConv::SETJMP: {
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
    case CallingConv::WIN64:
    case CallingConv::XEN: {
      llvm_unreachable("not implemented");
    }
    case CallingConv::INTR:
    case CallingConv::MULTIBOOT: {
      llvm_unreachable("cannot call interrupts");
    }
  }
  llvm_unreachable("invalid calling convention");
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<unsigned> X86_64Call::GetXMMs() const
{
  switch (conv_) {
    case CallingConv::C:
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
    case CallingConv::WIN64:
    case CallingConv::XEN: {
      llvm_unreachable("not implemented");
    }
    case CallingConv::INTR:
    case CallingConv::MULTIBOOT: {
      llvm_unreachable("cannot call interrupts");
    }
  }
  llvm_unreachable("invalid calling convention");
}
