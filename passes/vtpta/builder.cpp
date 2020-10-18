// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "passes/vtpta/builder.h"

using namespace vtpta;



// -----------------------------------------------------------------------------
SymExpr *Builder::BuildCall(const CallSite &call) {
  llvm_unreachable("Call");
}

// -----------------------------------------------------------------------------
void Builder::Build(const Func &func) {
  for (const Block &block : func) {
    for (const Inst &inst : block) {
      BuildFlow(inst);
    }
  }
}

// -----------------------------------------------------------------------------
void Builder::BuildConstraint(const Func &func) {
  for (const Block &block : func) {
    auto *term = block.GetTerminator();
    if (auto *switchInst = ::dyn_cast_or_null<const SwitchInst>(term)) {
      llvm_unreachable("SwitchInst");
      continue;
    }
    if (auto *inst = ::dyn_cast_or_null<const JumpInst>(term)) {
      llvm_unreachable("JumpInst");
      continue;
    }
    if (auto *inst = ::dyn_cast_or_null<const JumpCondInst>(term)) {
      llvm_unreachable("JumpCondInst");
      continue;
    }
    if (auto *inst = ::dyn_cast_or_null<const InvokeInst>(term)) {
      llvm_unreachable("InvokeInst");
      continue;
    }
  }
}

// -----------------------------------------------------------------------------
void Builder::BuildFlow(const Inst &inst) {
  switch (inst.GetKind()) {
    case Inst::Kind::CALL:
    case Inst::Kind::INVOKE: {
      BuildCall(static_cast<const CallSite &>(inst));
      return;
    }
    case Inst::Kind::TCALL: {
      auto &tail = static_cast<const CallSite &>(inst);
      if (auto *V = BuildCall(tail)) {
        BuildRet(tail);
      }
      return;
    }
    case Inst::Kind::SYSCALL: {
      llvm_unreachable("not implemented");
    }
    case Inst::Kind::CLONE: {
      llvm_unreachable("not implemented");
    }

    case Inst::Kind::RET: {
      for (auto *arg : static_cast<const ReturnInst &>(inst).args()) {
        BuildRet(*arg);
      }
      return;
    }

    case Inst::Kind::ARG:
      return BuildArg(static_cast<const ArgInst &>(inst));

    case Inst::Kind::SELECT:
      return BuildSelect(static_cast<const SelectInst &>(inst));

    case Inst::Kind::LD:
      return BuildLoad(static_cast<const LoadInst &>(inst));
    case Inst::Kind::ST:
      return BuildStore(static_cast<const StoreInst &>(inst));
    case Inst::Kind::X86_XCHG:
      return BuildX86_Xchg(static_cast<const X86_XchgInst &>(inst));
    case Inst::Kind::X86_CMPXCHG:
      llvm_unreachable("not implemented");

    case Inst::Kind::VASTART:
      return BuildVastart(static_cast<const VAStartInst &>(inst));
    case Inst::Kind::ALLOCA:
      return BuildAlloca(static_cast<const AllocaInst &>(inst));
    case Inst::Kind::FRAME:
      return BuildFrame(static_cast<const FrameInst &>(inst));

    case Inst::Kind::NEG:
      return BuildNeg(static_cast<const NegInst &>(inst));
    case Inst::Kind::TRUNC:
      return BuildTrunc(static_cast<const TruncInst &>(inst));
    case Inst::Kind::SEXT:
      return BuildSext(static_cast<const SExtInst &>(inst));
    case Inst::Kind::ZEXT:
      return BuildZext(static_cast<const ZExtInst &>(inst));
    case Inst::Kind::XEXT:
      return BuildZext(static_cast<const ZExtInst &>(inst));
    case Inst::Kind::FEXT:
      return BuildFext(static_cast<const FExtInst &>(inst));
    case Inst::Kind::ADD:
      return BuildAdd(static_cast<const AddInst &>(inst));
    case Inst::Kind::SUB:
      return BuildSub(static_cast<const SubInst &>(inst));
    case Inst::Kind::CMP:
      return BuildCmp(static_cast<const CmpInst &>(inst));
    case Inst::Kind::MUL:
      return BuildMul(static_cast<const MulInst &>(inst));
    case Inst::Kind::MOV:
      return BuildMov(static_cast<const MovInst &>(inst));
    case Inst::Kind::PHI:
      return BuildPhi(static_cast<const PhiInst &>(inst));

    case Inst::Kind::UNDEF:
    case Inst::Kind::EXP:
    case Inst::Kind::EXP2:
    case Inst::Kind::LOG:
    case Inst::Kind::LOG2:
    case Inst::Kind::LOG10:
    case Inst::Kind::FCEIL:
    case Inst::Kind::FFLOOR:
    case Inst::Kind::SQRT:
    case Inst::Kind::SIN:
    case Inst::Kind::COS:
    case Inst::Kind::POW:
    case Inst::Kind::COPYSIGN:
    case Inst::Kind::SADDO:
    case Inst::Kind::SMULO:
    case Inst::Kind::SSUBO:
    case Inst::Kind::UADDO:
    case Inst::Kind::UMULO:
    case Inst::Kind::USUBO:
    case Inst::Kind::AND:
    case Inst::Kind::UDIV:
    case Inst::Kind::SDIV:
    case Inst::Kind::UREM:
    case Inst::Kind::SREM:
    case Inst::Kind::OR:
    case Inst::Kind::ROTL:
    case Inst::Kind::ROTR:
    case Inst::Kind::SLL:
    case Inst::Kind::SRA:
    case Inst::Kind::SRL:
    case Inst::Kind::XOR:
    case Inst::Kind::ABS:
    case Inst::Kind::POPCNT:
    case Inst::Kind::CLZ:
    case Inst::Kind::CTZ:
    case Inst::Kind::X86_RDTSC:
      return BuildUnknown(inst);

    case Inst::Kind::JCC:
    case Inst::Kind::JMP:
    case Inst::Kind::SWITCH:
    case Inst::Kind::TRAP:
    case Inst::Kind::SET:
    case Inst::Kind::X86_FNSTCW:
    case Inst::Kind::X86_FNSTSW:
    case Inst::Kind::X86_FNSTENV:
    case Inst::Kind::X86_FLDCW:
    case Inst::Kind::X86_FLDENV:
    case Inst::Kind::X86_LDMXCSR:
    case Inst::Kind::X86_STMXCSR:
    case Inst::Kind::X86_FNCLEX:
      return;

    case Inst::Kind::RAISE:
      // TODO: connect to potential call sites.
      return;
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
void Builder::BuildRet(const Inst &inst) {
  llvm_unreachable("Ret");
}

// -----------------------------------------------------------------------------
void Builder::BuildArg(const ArgInst &inst) {
  llvm_unreachable("Arg");
}

// -----------------------------------------------------------------------------
void Builder::BuildSelect(const SelectInst &inst) {
  llvm_unreachable("Select");
}

// -----------------------------------------------------------------------------
void Builder::BuildLoad(const LoadInst &inst) {
  llvm_unreachable("Load");
}

// -----------------------------------------------------------------------------
void Builder::BuildStore(const StoreInst &inst) {
  llvm_unreachable("Store");
}

// -----------------------------------------------------------------------------
void Builder::BuildX86_Xchg(const X86_XchgInst &inst) {
  llvm_unreachable("Xchg");
}

// -----------------------------------------------------------------------------
void Builder::BuildVastart(const VAStartInst &inst) {
  llvm_unreachable("Vastart");
}

// -----------------------------------------------------------------------------
void Builder::BuildAlloca(const AllocaInst &inst) {
  llvm_unreachable("Alloca");
}

// -----------------------------------------------------------------------------
void Builder::BuildFrame(const FrameInst &inst) {
  llvm_unreachable("Frame");
}

// -----------------------------------------------------------------------------
void Builder::BuildNeg(const NegInst &inst) {
  llvm_unreachable("Neg");
}

// -----------------------------------------------------------------------------
void Builder::BuildTrunc(const TruncInst &inst) {
  llvm_unreachable("Trunc");
}

// -----------------------------------------------------------------------------
void Builder::BuildSext(const SExtInst &inst) {
  llvm_unreachable("Sext");
}

// -----------------------------------------------------------------------------
void Builder::BuildZext(const ZExtInst &inst) {
  llvm_unreachable("Zext");
}

// -----------------------------------------------------------------------------
void Builder::BuildFext(const FExtInst &inst) {
  llvm_unreachable("Fext");
}

// -----------------------------------------------------------------------------
void Builder::BuildAdd(const AddInst &inst) {
  llvm_unreachable("Add");
}

// -----------------------------------------------------------------------------
void Builder::BuildSub(const SubInst &inst) {
  llvm_unreachable("Sub");
}

// -----------------------------------------------------------------------------
void Builder::BuildCmp(const CmpInst &inst) {
  llvm_unreachable("Cmp");
}

// -----------------------------------------------------------------------------
void Builder::BuildMul(const MulInst &inst) {
  llvm_unreachable("Mul");
}

// -----------------------------------------------------------------------------
void Builder::BuildMov(const MovInst &inst) {
  llvm_unreachable("Mov");
}

// -----------------------------------------------------------------------------
void Builder::BuildPhi(const PhiInst &inst) {
  llvm_unreachable("Phi");
}

// -----------------------------------------------------------------------------
void Builder::BuildUnknown(const Inst &inst) {
  llvm_unreachable("Unknown");
}
