// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"
#include "core/inst_visitor.h"

class Func;
class MovInst;



/**
 * Pass to eliminate unnecessary moves.
 */
class VerifierPass final : public Pass, ConstInstVisitor<void> {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  VerifierPass(PassManager *passManager);

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Verifies a function.
  void Verify(Func &func);

  /// Ensure a type is an integer.
  void CheckInteger(
      const Inst &inst,
      ConstRef<Inst> ref,
      const char *msg = "not an integer"
  );

  /// Ensure a type is a pointer.
  void CheckPointer(
      const Inst &inst,
      ConstRef<Inst> ref,
      const char *msg = "not a pointer"
  );

  /// Ensure a type is compatible with a given one.
  void CheckType(
      const Inst &inst,
      ConstRef<Inst> ref,
      Type type
  );
  /// Report an error.
  [[noreturn]] void Error(const Inst &i, llvm::Twine msg);

private:
  void VisitInst(const Inst &i) override { }
  void VisitConstInst(const ConstInst &i) override;
  void VisitOperatorInst(const OperatorInst &i) override {}
  void VisitUnaryInst(const UnaryInst &i) override;
  void VisitConversionInst(const ConversionInst &i) override {}
  void VisitBinaryInst(const BinaryInst &i) override;
  void VisitOverflowInst(const OverflowInst &i) override;
  void VisitShiftRotateInst(const ShiftRotateInst &i) override;
  void VisitDivisionInst(const DivisionInst &i) override {}
  void VisitMemoryInst(const MemoryInst &i) override;
  void VisitBarrierInst(const BarrierInst &i) override;
  void VisitMemoryExchangeInst(const MemoryExchangeInst &i) override;
  void VisitMemoryCompareExchangeInst(const MemoryCompareExchangeInst &i) override;
  void VisitLoadLinkInst(const LoadLinkInst &i) override;
  void VisitStoreCondInst(const StoreCondInst &i) override;
  void VisitControlInst(const ControlInst &i) override {}
  void VisitTerminatorInst(const TerminatorInst &i) override {}
  void VisitCallSite(const CallSite &i) override;
  void VisitX86_FPUControlInst(const X86_FPUControlInst &i) override;
  void VisitPhiInst(const PhiInst &i) override;
  void VisitMovInst(const MovInst &i) override;
  void VisitAllocaInst(const AllocaInst &i) override;
  void VisitFrameInst(const FrameInst &i) override;
  void VisitSetInst(const SetInst &i) override;
  void VisitGetInst(const GetInst &i) override;
  void VisitCmpInst(const CmpInst &i) override;
  void VisitSyscallInst(const SyscallInst &i) override;
  void VisitArgInst(const ArgInst &i) override;
  void VisitRaiseInst(const RaiseInst &i) override;
  void VisitLandingPadInst(const LandingPadInst &i) override;
  void VisitLoadInst(const LoadInst &i) override;
  void VisitStoreInst(const StoreInst &i) override;
  void VisitVaStartInst(const VaStartInst &i) override;
  void VisitSelectInst(const SelectInst &i) override;

private:
  /// Underlying pointer type.
  Type ptrTy_;
};
