// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/insts.h"



/**
 * Helper class to build custom visitors for all IR instructions.
 */
template<typename T>
class InstVisitor {
public:
  virtual ~InstVisitor() {}

  T Dispatch(Inst &i)
  {
    switch (i.GetKind()) {
      #define GET_INST(kind, type, name, sort) \
        case Inst::Kind::kind: return Visit##type(static_cast<type &>(i));
      #include "instructions.def"
    }
    llvm_unreachable("invalid instruction kind");
  }

public:
  virtual T VisitInst(Inst &i) = 0;
  virtual T VisitConstInst(ConstInst &i) { return VisitInst(i); }
  virtual T VisitOperatorInst(OperatorInst &i) { return VisitInst(i); }
  virtual T VisitUnaryInst(UnaryInst &i) { return VisitOperatorInst(i); }
  virtual T VisitConversionInst(ConversionInst &i) { return VisitUnaryInst(i); }
  virtual T VisitBinaryInst(BinaryInst &i) { return VisitOperatorInst(i); }
  virtual T VisitOverflowInst(OverflowInst &i) { return VisitBinaryInst(i); }
  virtual T VisitShiftRotateInst(ShiftRotateInst &i) { return VisitBinaryInst(i); }
  virtual T VisitRotateInst(RotateInst &i) { return VisitShiftRotateInst(i); }
  virtual T VisitShiftInst(ShiftInst &i) { return VisitShiftRotateInst(i); }
  virtual T VisitDivisionInst(DivisionInst &i) { return VisitBinaryInst(i); }
  virtual T VisitMemoryInst(MemoryInst &i) { return VisitInst(i); }
  virtual T VisitBarrierInst(BarrierInst &i) { return VisitMemoryInst(i); }
  virtual T VisitMemoryLoadInst(MemoryLoadInst &i) { return VisitMemoryInst(i); }
  virtual T VisitMemoryStoreInst(MemoryStoreInst &i) { return VisitMemoryInst(i); }
  virtual T VisitMemoryExchangeInst(MemoryExchangeInst &i) { return VisitMemoryInst(i); }
  virtual T VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i) { return VisitMemoryExchangeInst(i); }
  virtual T VisitLoadLinkInst(LoadLinkInst &i) { return VisitMemoryInst(i); }
  virtual T VisitStoreCondInst(StoreCondInst &i) { return VisitMemoryInst(i); }
  virtual T VisitControlInst(ControlInst &i) { return VisitInst(i); }
  virtual T VisitTerminatorInst(TerminatorInst &i) { return VisitControlInst(i); }
  virtual T VisitCallSite(CallSite &i) { return VisitTerminatorInst(i); }
  virtual T VisitX86_FPUControlInst(X86_FPUControlInst &i) { return VisitInst(i); }
  virtual T VisitX86_ContextInst(X86_ContextInst &i) { return VisitInst(i); }

public:
  #define GET_INST(kind, type, name, sort) \
    virtual T Visit##type(type &i) { return Visit##sort(i); }
  #include "instructions.def"
};

/**
 * Helper class to build custom visitors for all IR instructions.
 */
template<typename T>
class ConstInstVisitor {
public:
  virtual ~ConstInstVisitor() {}

  T Dispatch(const Inst &i)
  {
    switch (i.GetKind()) {
      #define GET_INST(kind, type, name, sort) \
        case Inst::Kind::kind: return Visit##type(static_cast<const type &>(i));
      #include "instructions.def"
    }
    llvm_unreachable("invalid instruction kind");
  }

public:
  virtual T VisitInst(const Inst &i) = 0;
  virtual T VisitConstInst(const ConstInst &i) { return VisitInst(i); }
  virtual T VisitOperatorInst(const OperatorInst &i) { return VisitInst(i); }
  virtual T VisitUnaryInst(const UnaryInst &i) { return VisitOperatorInst(i); }
  virtual T VisitConversionInst(const ConversionInst &i) { return VisitUnaryInst(i); }
  virtual T VisitBinaryInst(const BinaryInst &i) { return VisitOperatorInst(i); }
  virtual T VisitOverflowInst(const OverflowInst &i) { return VisitBinaryInst(i); }
  virtual T VisitShiftRotateInst(const ShiftRotateInst &i) { return VisitBinaryInst(i); }
  virtual T VisitRotateInst(const RotateInst &i) { return VisitShiftRotateInst(i); }
  virtual T VisitShiftInst(const ShiftInst &i) { return VisitShiftRotateInst(i); }
  virtual T VisitDivisionInst(const DivisionInst &i) { return VisitBinaryInst(i); }
  virtual T VisitMemoryInst(const MemoryInst &i) { return VisitInst(i); }
  virtual T VisitBarrierInst(const BarrierInst &i) { return VisitMemoryInst(i); }
  virtual T VisitMemoryLoadInst(const MemoryLoadInst &i) { return VisitMemoryInst(i); }
  virtual T VisitMemoryStoreInst(const MemoryStoreInst &i) { return VisitMemoryInst(i); }
  virtual T VisitMemoryExchangeInst(const MemoryExchangeInst &i) { return VisitMemoryInst(i); }
  virtual T VisitMemoryCompareExchangeInst(const MemoryCompareExchangeInst &i) { return VisitMemoryExchangeInst(i); }
  virtual T VisitLoadLinkInst(const LoadLinkInst &i) { return VisitMemoryInst(i); }
  virtual T VisitStoreCondInst(const StoreCondInst &i) { return VisitMemoryInst(i); }
  virtual T VisitControlInst(const ControlInst &i) { return VisitInst(i); }
  virtual T VisitTerminatorInst(const TerminatorInst &i) { return VisitControlInst(i); }
  virtual T VisitCallSite(const CallSite &i) { return VisitTerminatorInst(i); }
  virtual T VisitX86_FPUControlInst(const X86_FPUControlInst &i) { return VisitInst(i); }
  virtual T VisitX86_ContextInst(const X86_ContextInst &i) { return VisitInst(i); }

public:
  #define GET_INST(kind, type, name, sort) \
    virtual T Visit##type(const type &i) { return Visit##sort(i); }
  #include "instructions.def"
};
