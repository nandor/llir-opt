// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"
#include "core/target.h"



namespace tags {

class RegisterAnalysis;
class TaggedType;



/**
 * Helper to produce the initial types for known values.
 */
class Init : public InstVisitor<void> {
public:
  Init(RegisterAnalysis &analysis, const Target *target)
    : analysis_(analysis)
    , target_(target)
  {
  }

  void VisitArgInst(ArgInst &i) override;
  void VisitMovInst(MovInst &i) override;
  void VisitNegInst(NegInst &i) override;
  void VisitFrameInst(FrameInst &i) override;
  void VisitAllocaInst(AllocaInst &i) override;
  void VisitGetInst(GetInst &i) override;
  void VisitUndefInst(UndefInst &i) override;
  void VisitCopySignInst(CopySignInst &i) override;
  void VisitFloatInst(FloatInst &i) override;
  void VisitX86_RdTscInst(X86_RdTscInst &i) override;
  void VisitLoadInst(LoadInst &i) override;
  void VisitBitCountInst(BitCountInst &i) override;
  void VisitRotateInst(RotateInst &i) override;
  void VisitSyscallInst(SyscallInst &i) override;
  void VisitCloneInst(CloneInst &i) override;
  void VisitLandingPadInst(LandingPadInst &i) override;

  void VisitControlInst(ControlInst &i) override {}
  void VisitBarrierInst(BarrierInst &i) override {}
  void VisitX86_PauseInst(X86_PauseInst &i) override {}
  void VisitX86_YieldInst(X86_YieldInst &i) override {}
  void VisitX86_BarrierInst(X86_BarrierInst &i) override {}
  void VisitX86_HltInst(X86_HltInst &i) override {}
  void VisitX86_FnClExInst(X86_FnClExInst &i) override {}
  void VisitX86_FPUControlInst(X86_FPUControlInst &i) override {}

  void VisitInst(Inst &i) override
  {
    for (auto v : i.operand_values()) {
      if (v->Is(Value::Kind::INST)) {
        return;
      }
    }
    i.dump();
    llvm_unreachable("instruction not handled");
  }

private:
  TaggedType Infer(Type ty);

private:
  /// Reference to the analysis.
  RegisterAnalysis &analysis_;
  /// Reference to target info.
  const Target *target_;
};

} // end namespace
