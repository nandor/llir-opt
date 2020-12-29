// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"

class SymbolicContext;
class SymbolicHeap;



/**
 * Symbolically evaluate an instruction.
 */
class SymbolicEval final : public InstVisitor<void> {
public:
  SymbolicEval(
      SymbolicContext &ctx,
      SymbolicHeap &heap)
    : ctx_(ctx)
    , heap_(heap)
  {
  }

  virtual void VisitInst(Inst &i) override;
  virtual void VisitMovGlobal(Inst &i, Global &g, int64_t offset);
  virtual void VisitBarrierInst(BarrierInst &i) override;
  virtual void VisitMemoryLoadInst(MemoryLoadInst &i) override;
  virtual void VisitMemoryStoreInst(MemoryStoreInst &i) override;
  virtual void VisitMemoryExchangeInst(MemoryExchangeInst &i) override;
  virtual void VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i) override;
  virtual void VisitLoadLinkInst(LoadLinkInst &i) override;
  virtual void VisitStoreCondInst(StoreCondInst &i) override;
  virtual void VisitTerminatorInst(TerminatorInst &i) override { }

  virtual void VisitArgInst(ArgInst &i) override;
  virtual void VisitMovInst(MovInst &i) override;
  virtual void VisitSllInst(SllInst &i) override;
  virtual void VisitAddInst(AddInst &i) override;
  virtual void VisitAndInst(AndInst &i) override;
  virtual void VisitX86_WrMsrInst(X86_WrMsrInst &i) override;
  virtual void VisitX86_RdTscInst(X86_RdTscInst &i) override;

private:
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
  /// Reference to the symbolic heap.
  SymbolicHeap &heap_;
};
