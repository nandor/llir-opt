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



// -----------------------------------------------------------------------------
X86Call::X86Call(const Func *func)
  : stack_(0ull)
  , args_(func->GetNumFixedArgs())
  , regs_(0)
  , xmms_(0)
{
  unsigned nargs = func->GetNumFixedArgs();
  std::vector<std::optional<Type>> argTys(nargs);
  for (const Block &block : *func) {
    for (const Inst &inst : block) {
      if (inst.GetKind() != Inst::Kind::ARG) {
        continue;
      }

      auto &argInst = static_cast<const ArgInst &>(inst);
      if (argInst.GetIdx() >= nargs) {
        throw std::runtime_error("Function declared fewer args");
      }
      argTys[argInst.GetIdx()] = argInst.GetType();
    }
  }

  for (unsigned i = 0; i < nargs; ++i) {
    if (!argTys[i]) {
      continue;
    }
    switch (func->GetCallingConv()) {
      case CallingConv::C:     AssignC(i, *argTys[i], nullptr); break;
      case CallingConv::FAST:  assert(!"not implemented"); break;
      case CallingConv::OCAML: assert(!"not implemented"); break;
    }
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
      if (regs_ < kArgI32.size()) {
        args_[i].Kind = Loc::Kind::REG;
        args_[i].Reg = kArgI32[regs_];
        args_[i].Type = type;
        args_[i].Value = value;
        regs_++;
      } else {
        assert(!"not implemented");
      }
      break;
    }
    case Type::U64: case Type::I64: {
      if (regs_ < kArgI64.size()) {
        args_[i].Kind = Loc::Kind::REG;
        args_[i].Reg = kArgI64[regs_];
        args_[i].Type = type;
        args_[i].Value = value;
        regs_++;
      } else {
        assert(!"not implemented");
      }
      break;
    }
    case Type::F32: case Type::F64: {
      if (xmms_ < kArgF.size()) {
        args_[i].Kind = Loc::Kind::REG;
        args_[i].Reg = kArgF[xmms_];
        args_[i].Type = type;
        args_[i].Value = value;
        xmms_++;
      } else {
        assert(!"not implemented");
      }
      break;
    }
  }
}
