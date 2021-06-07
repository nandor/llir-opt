// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"
#include "core/target.h"
#include "passes/tags/tagged_type.h"



namespace tags {

class RegisterAnalysis;

/**
 * Helper class to evaluate instructions and propagate values.
 */
class Step final : public InstVisitor<void> {
public:
  enum class Kind {
    REFINE,
    FORWARD,
  };

public:
  Step(RegisterAnalysis &analysis, const Target *target, Kind kind)
    : analysis_(analysis)
    , target_(target)
    , kind_(kind)
  {}

private:
  void VisitCallSite(CallSite &i) override;
  void VisitMovInst(MovInst &i) override;
  void VisitAddInst(AddInst &i) override;
  void VisitSubInst(SubInst &i) override;
  void VisitMulInst(MulInst &i) override;
  void VisitMultiplyInst(MultiplyInst &i) override;
  void VisitDivisionRemainderInst(DivisionRemainderInst &i) override;
  void VisitAndInst(AndInst &i) override;
  void VisitXorInst(XorInst &i) override;
  void VisitOrInst(OrInst &i) override;
  void VisitShiftRightInst(ShiftRightInst &i) override;
  void VisitSllInst(SllInst &i) override;
  void VisitRotlInst(RotlInst &i) override;
  void VisitExtensionInst(ExtensionInst &i) override;
  void VisitTruncInst(TruncInst &i) override;
  void VisitBitCastInst(BitCastInst &i) override;
  void VisitByteSwapInst(ByteSwapInst &i) override;
  void VisitMemoryExchangeInst(MemoryExchangeInst &i) override;
  void VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i) override;
  void VisitCmpInst(CmpInst &i) override;
  void VisitSelectInst(SelectInst &i) override;
  void VisitPhiInst(PhiInst &i) override;
  void VisitReturnInst(ReturnInst &i) override;

  // Instructions with no effect.
  void VisitTerminatorInst(TerminatorInst &i) override {}
  void VisitSetInst(SetInst &i) override {}
  void VisitX86_OutInst(X86_OutInst &i) override {}
  void VisitX86_WrMsrInst(X86_WrMsrInst &i) override {}
  void VisitX86_LidtInst(X86_LidtInst &i) override {}
  void VisitX86_LgdtInst(X86_LgdtInst &i) override {}
  void VisitX86_LtrInst(X86_LtrInst &i) override {}
  void VisitX86_FPUControlInst(X86_FPUControlInst &i) override {}

  // Values do not change since init.
  void VisitLoadInst(LoadInst &i) override {}
  void VisitBitCountInst(BitCountInst &i) override {}
  void VisitVaStartInst(VaStartInst &i) override {}
  void VisitFrameInst(FrameInst &i) override {}
  void VisitAllocaInst(AllocaInst &i) override {}
  void VisitGetInst(GetInst &i) override {}
  void VisitUndefInst(UndefInst &i) override {}
  void VisitCopySignInst(CopySignInst &i) override {}
  void VisitFloatInst(FloatInst &i) override {}
  void VisitX86_RdTscInst(X86_RdTscInst &i) override {}
  void VisitStoreInst(StoreInst &i) override {}
  void VisitNegInst(NegInst &i) override {}
  void VisitRotateInst(RotateInst &i) override {}
  void VisitSyscallInst(SyscallInst &i) override {}
  void VisitCloneInst(CloneInst &i) override {}

  // All instruction classes should be handled.
  void VisitInst(Inst &i) override;

private:
  TaggedType Infer(Type ty);
  TaggedType Clamp(TaggedType type, Type ty);
  TaggedType Add(TaggedType vl, TaggedType vr);
  TaggedType Mul(TaggedType vl, TaggedType vr);
  TaggedType Sub(TaggedType vl, TaggedType vr);
  TaggedType And(Type ty, TaggedType vl, TaggedType vr);
  TaggedType Xor(TaggedType vl, TaggedType vr);
  TaggedType Or(TaggedType vl, TaggedType vr);
  TaggedType Shr(Type ty, TaggedType vl, TaggedType vr);
  TaggedType Shl(Type ty, TaggedType vl, TaggedType vr);
  TaggedType Ext(Type ty, TaggedType arg);
  TaggedType Trunc(Type ty, TaggedType arg);

private:
  /// Mark an instruction with a type.
  bool Mark(Ref<Inst> inst, const TaggedType &type);
  /// Return values through tail calls.
  void Return(
      Func *from,
      const Inst *inst,
      const std::vector<TaggedType> &values
  );
private:
  /// Reference to the analysis.
  RegisterAnalysis &analysis_;
  /// Reference to target info.
  const Target *target_;
  /// Operation mode.
  Kind kind_;
};

} // end namespace
