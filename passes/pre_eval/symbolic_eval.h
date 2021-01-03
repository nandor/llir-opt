// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"

class SymbolicContext;
class SymbolicHeap;
class ReferenceGraph;
class EvalContext;



/**
 * Symbolically evaluate an instruction.
 */
class SymbolicEval final : public InstVisitor<bool> {
public:
  SymbolicEval(EvalContext &eval, ReferenceGraph &refs, SymbolicContext &ctx)
    : eval_(eval)
    , refs_(refs)
    , ctx_(ctx)
  {
  }

  bool Evaluate(Inst &i);

private:
  bool VisitInst(Inst &i) override;
  bool VisitMovGlobal(Inst &i, Global &g, int64_t offset);
  bool VisitBarrierInst(BarrierInst &i) override;
  bool VisitMemoryLoadInst(MemoryLoadInst &i) override;
  bool VisitMemoryStoreInst(MemoryStoreInst &i) override;
  bool VisitMemoryExchangeInst(MemoryExchangeInst &i) override;
  bool VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i) override;
  bool VisitTerminatorInst(TerminatorInst &i) override;
  bool VisitVaStartInst(VaStartInst &i) override;
  bool VisitPhiInst(PhiInst &i) override;
  bool VisitArgInst(ArgInst &i) override;
  bool VisitMovInst(MovInst &i) override;
  bool VisitUndefInst(UndefInst &i) override;
  bool VisitFrameInst(FrameInst &i) override;
  bool VisitTruncInst(TruncInst &i) override;
  bool VisitZExtInst(ZExtInst &i) override;
  bool VisitSExtInst(SExtInst &i) override;
  bool VisitSllInst(SllInst &i) override;
  bool VisitSrlInst(SrlInst &i) override;
  bool VisitAddInst(AddInst &i) override;
  bool VisitAndInst(AndInst &i) override;
  bool VisitSubInst(SubInst &i) override;
  bool VisitOrInst(OrInst &i) override;
  bool VisitMulInst(MulInst &i) override;
  bool VisitUDivInst(UDivInst &i) override;
  bool VisitSDivInst(SDivInst &i) override;
  bool VisitCmpInst(CmpInst &i) override;
  bool VisitSelectInst(SelectInst &i) override;
  bool VisitOUMulInst(OUMulInst &i) override;
  bool VisitX86_OutInst(X86_OutInst &i) override;
  bool VisitX86_LgdtInst(X86_LgdtInst &i) override;
  bool VisitX86_LidtInst(X86_LidtInst &i) override;
  bool VisitX86_LtrInst(X86_LtrInst &i) override;
  bool VisitX86_SetCsInst(X86_SetCsInst &i) override;
  bool VisitX86_SetDsInst(X86_SetDsInst &i) override;
  bool VisitX86_WrMsrInst(X86_WrMsrInst &i) override;
  bool VisitX86_RdTscInst(X86_RdTscInst &i) override;

private:
  /// Context - information about data flow in the current function.
  EvalContext &eval_;
  /// Graph to approximate symbols referenced by functions.
  ReferenceGraph &refs_;
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
};
