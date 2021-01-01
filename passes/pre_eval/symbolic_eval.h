// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"

class SymbolicContext;
class SymbolicHeap;
class ReferenceGraph;


/**
 * Symbolically evaluate an instruction.
 */
class SymbolicEval final : public InstVisitor<bool> {
public:
  SymbolicEval(ReferenceGraph &refs, SymbolicContext &ctx)
    : refs_(refs)
    , ctx_(ctx)
  {
  }

  bool Evaluate(Inst &i);

private:
  virtual bool VisitInst(Inst &i) override;
  virtual bool VisitMovGlobal(Inst &i, Global &g, int64_t offset);
  virtual bool VisitBarrierInst(BarrierInst &i) override;
  virtual bool VisitMemoryLoadInst(MemoryLoadInst &i) override;
  virtual bool VisitMemoryStoreInst(MemoryStoreInst &i) override;
  virtual bool VisitMemoryExchangeInst(MemoryExchangeInst &i) override;
  virtual bool VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i) override;
  virtual bool VisitLoadLinkInst(LoadLinkInst &i) override;
  virtual bool VisitStoreCondInst(StoreCondInst &i) override;
  virtual bool VisitTerminatorInst(TerminatorInst &i) override { return false; }

  virtual bool VisitCallSite(CallSite &i) override;

  virtual bool VisitPhiInst(PhiInst &i) override;
  virtual bool VisitArgInst(ArgInst &i) override;
  virtual bool VisitMovInst(MovInst &i) override;
  virtual bool VisitFrameInst(FrameInst &i) override;
  virtual bool VisitTruncInst(TruncInst &i) override;
  virtual bool VisitZExtInst(ZExtInst &i) override;
  virtual bool VisitSExtInst(SExtInst &i) override;
  virtual bool VisitSllInst(SllInst &i) override;
  virtual bool VisitSrlInst(SrlInst &i) override;
  virtual bool VisitAddInst(AddInst &i) override;
  virtual bool VisitAndInst(AndInst &i) override;
  virtual bool VisitSubInst(SubInst &i) override;
  virtual bool VisitOrInst(OrInst &i) override;
  virtual bool VisitMulInst(MulInst &i) override;
  virtual bool VisitUDivInst(UDivInst &i) override;
  virtual bool VisitCmpInst(CmpInst &i) override;
  virtual bool VisitSelectInst(SelectInst &i) override;
  virtual bool VisitX86_OutInst(X86_OutInst &i) override;
  virtual bool VisitX86_LgdtInst(X86_LgdtInst &i) override;
  virtual bool VisitX86_LidtInst(X86_LidtInst &i) override;
  virtual bool VisitX86_LtrInst(X86_LtrInst &i) override;
  virtual bool VisitX86_SetCsInst(X86_SetCsInst &i) override;
  virtual bool VisitX86_SetDsInst(X86_SetDsInst &i) override;
  virtual bool VisitX86_WrMsrInst(X86_WrMsrInst &i) override;
  virtual bool VisitX86_RdTscInst(X86_RdTscInst &i) override;

private:
  /// Graph to approximate symbols referenced by functions.
  ReferenceGraph &refs_;
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
};
