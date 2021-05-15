// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"
#include "core/target.h"



namespace tags {

class TypeAnalysis;

/**
 * Helper to produce the initial types for known values.
 */
class Init : public ConstInstVisitor<void> {
public:
  Init(TypeAnalysis &analysis, const Target *target)
    : analysis_(analysis)
    , target_(target)
  {
  }

  void VisitArgInst(const ArgInst &i) override;
  void VisitMovInst(const MovInst &i) override;
  void VisitNegInst(const NegInst &i) override;
  void VisitFrameInst(const FrameInst &i) override;
  void VisitAllocaInst(const AllocaInst &i) override;
  void VisitGetInst(const GetInst &i) override;
  void VisitUndefInst(const UndefInst &i) override;
  void VisitX86_RdTscInst(const X86_RdTscInst &i) override;
  void VisitCmpInst(const CmpInst &i) override;
  void VisitLoadInst(const LoadInst &i) override;
  void VisitBitCountInst(const BitCountInst &i) override;
  void VisitRotateInst(const RotateInst &i) override;
  void VisitSyscallInst(const SyscallInst &i) override;

  void VisitControlInst(const ControlInst &i) override {}
  void VisitBarrierInst(const BarrierInst &i) override {}
  void VisitX86_PauseInst(const X86_PauseInst &i) override {}
  void VisitX86_HltInst(const X86_HltInst &i) override {}

  void VisitInst(const Inst &i) override
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
  /// Reference to the analysis.
  TypeAnalysis &analysis_;
  /// Reference to target info.
  const Target *target_;
};

} // end namespace
