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
class SymbolicEval final : public InstVisitor<bool> {
public:
  SymbolicEval(
      SymbolicContext &ctx,
      SymbolicHeap &heap)
    : ctx_(ctx)
    , heap_(heap)
  {
  }

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

  virtual bool VisitArgInst(ArgInst &i) override;
  virtual bool VisitMovInst(MovInst &i) override;
  virtual bool VisitSllInst(SllInst &i) override;
  virtual bool VisitAddInst(AddInst &i) override;
  virtual bool VisitAndInst(AndInst &i) override;
  virtual bool VisitX86_WrMsrInst(X86_WrMsrInst &i) override;
  virtual bool VisitX86_RdTscInst(X86_RdTscInst &i) override;

private:
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
  /// Reference to the symbolic heap.
  SymbolicHeap &heap_;
};
