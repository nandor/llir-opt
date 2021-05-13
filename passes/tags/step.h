// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"
#include "core/target.h"
#include "passes/tags/tagged_type.h"



namespace tags {

class TypeAnalysis;

/**
 * Helper to propagate types.
 */
class Step : public ConstInstVisitor<void> {
public:
  Step(TypeAnalysis &analysis, const Target *target)
    : analysis_(analysis)
    , target_(target)
  {
  }

  void VisitCallSite(const CallSite &i) override;
  void VisitMovInst(const MovInst &i) override;
  void VisitAddInst(const AddInst &i) override;
  void VisitSubInst(const SubInst &i) override;
  void VisitMultiplyInst(const MultiplyInst &i) override;
  void VisitDivisionRemainderInst(const DivisionRemainderInst &i) override;
  void VisitAndInst(const AndInst &i) override;
  void VisitXorInst(const XorInst &i) override;
  void VisitOrInst(const OrInst &i) override;
  void VisitShiftRightInst(const ShiftRightInst &i) override;
  void VisitSllInst(const SllInst &i) override;
  void VisitRotlInst(const RotlInst &i) override;
  void VisitExtensionInst(const ExtensionInst &i) override;
  void VisitTruncInst(const TruncInst &i) override;
  void VisitBitCastInst(const BitCastInst &i) override;
  void VisitByteSwapInst(const ByteSwapInst &i) override;
  void VisitMemoryCompareExchangeInst(const MemoryCompareExchangeInst &i) override;
  void VisitSelectInst(const SelectInst &i) override;
  void VisitPhiInst(const PhiInst &i) override;
  void VisitReturnInst(const ReturnInst &i) override;

  // Instructions with no effect.
  void VisitTerminatorInst(const TerminatorInst &i) override {}
  void VisitSetInst(const SetInst &i) override {}
  void VisitX86_OutInst(const X86_OutInst &i) override {}
  void VisitX86_WrMsrInst(const X86_WrMsrInst &i) override {}
  void VisitX86_LidtInst(const X86_LidtInst &i) override {}
  void VisitX86_LgdtInst(const X86_LgdtInst &i) override {}
  void VisitX86_LtrInst(const X86_LtrInst &i) override {}

  // Values do not change since init.
  void VisitBitCountInst(const BitCountInst &i) override {}
  void VisitVaStartInst(const VaStartInst &i) override {}
  void VisitFrameInst(const FrameInst &i) override {}
  void VisitAllocaInst(const AllocaInst &i) override {}
  void VisitGetInst(const GetInst &i) override {}
  void VisitUndefInst(const UndefInst &i) override {}
  void VisitX86_RdTscInst(const X86_RdTscInst &i) override {}
  void VisitCmpInst(const CmpInst &i) override {}
  void VisitStoreInst(const StoreInst &i) override {}
  void VisitLoadInst(const LoadInst &i) override {}
  void VisitNegInst(const NegInst &i) override {}
  void VisitRotateInst(const RotateInst &i) override {}

  // All instruction classes should be handled.
  void VisitInst(const Inst &i) override
  {
    llvm::errs() << i << "\n";
    llvm_unreachable("not implemented");
  }

private:
  /// Return values through tail calls.
  void Return(const Func *from, const std::vector<TaggedType> &values);

private:
  /// Reference to the analysis.
  TypeAnalysis &analysis_;
  /// Reference to target info.
  const Target *target_;
};

} // end namespace
