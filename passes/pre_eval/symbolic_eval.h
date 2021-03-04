// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"

class SymbolicContext;
class SymbolicHeap;
class ReferenceGraph;
class SymbolicFrame;



/**
 * Symbolically evaluate an instruction.
 */
class SymbolicEval final : public InstVisitor<bool> {
public:
  SymbolicEval(
      SymbolicHeap &heap,
      SymbolicFrame &frame,
      ReferenceGraph &refs,
      SymbolicContext &ctx,
      Inst &inst)
    : heap_(heap)
    , frame_(frame)
    , refs_(refs)
    , ctx_(ctx)
    , inst_(inst)
  {
  }

  bool Evaluate();

public:
  /// Return the context.
  SymbolicContext &GetContext() { return ctx_; }

  /// Find a value.
  const SymbolicValue &Find(ConstRef<Inst> inst) { return frame_.Find(inst); }

  /// Helper to return a scalar.
  bool SetUndefined()
  {
    return frame_.Set(inst_, SymbolicValue::Undefined(GetOrigin()));
  }

  /// Helper to return a scalar.
  bool SetScalar()
  {
    return frame_.Set(inst_, SymbolicValue::Scalar(GetOrigin()));
  }

  /// Helper to return an integer.
  bool SetInteger(const APInt &i)
  {
    return frame_.Set(inst_, SymbolicValue::Integer(i, GetOrigin()));
  }

  /// Helper to return a float.
  bool SetFloat(const APFloat &i)
  {
    return frame_.Set(inst_, SymbolicValue::Float(i, GetOrigin()));
  }

  /// Helper to return a lower bounded integer.
  bool SetLowerBounded(const APInt &i)
  {
    return frame_.Set(inst_, SymbolicValue::LowerBoundedInteger(i, GetOrigin()));
  }

  /// Forward to frameuator, return a pointer.
  bool SetMask(const APInt &k, const APInt &v)
  {
    return frame_.Set(inst_, SymbolicValue::Mask(k, v, GetOrigin()));
  }

  /// Helper to forward a pointer (value).
  bool SetValue(const SymbolicPointer::Ref &ptr)
  {
    return frame_.Set(inst_, SymbolicValue::Value(ptr, GetOrigin()));
  }

  /// Helper to forward a pointer (pointer).
  bool SetPointer(const SymbolicPointer::Ref &ptr)
  {
    return frame_.Set(inst_, SymbolicValue::Pointer(ptr, GetOrigin()));
  }

  /// Helper to forward a pointer (nullptr).
  bool SetNullable(const SymbolicPointer::Ref &ptr)
  {
    return frame_.Set(inst_, SymbolicValue::Nullable(ptr, GetOrigin()));
  }

  /// Helper to set a value.
  bool NOP(const SymbolicValue &value)
  {
    return frame_.Set(inst_, value);
  }

private:
  bool VisitInst(Inst &i) override;
  bool VisitBarrierInst(BarrierInst &i) override;
  bool VisitMemoryLoadInst(MemoryLoadInst &i) override;
  bool VisitMemoryStoreInst(MemoryStoreInst &i) override;
  bool VisitMemoryExchangeInst(MemoryExchangeInst &i) override;
  bool VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i) override;
  bool VisitVaStartInst(VaStartInst &i) override;
  bool VisitArgInst(ArgInst &i) override;
  bool VisitMovInst(MovInst &i) override;
  bool VisitBitCastInst(BitCastInst &i) override;
  bool VisitUndefInst(UndefInst &i) override;
  bool VisitFrameInst(FrameInst &i) override;
  bool VisitTruncInst(TruncInst &i) override;
  bool VisitZExtInst(ZExtInst &i) override;
  bool VisitSExtInst(SExtInst &i) override;

  bool VisitSllInst(SllInst &i) override;
  bool VisitSrlInst(SrlInst &i) override;
  bool VisitSraInst(SraInst &i) override;

  bool VisitAndInst(AndInst &i) override;
  bool VisitOrInst(OrInst &i) override;
  bool VisitXorInst(XorInst &i) override;

  bool VisitAddInst(AddInst &i) override;
  bool VisitSubInst(SubInst &i) override;
  bool VisitUDivInst(UDivInst &i) override;
  bool VisitSDivInst(SDivInst &i) override;
  bool VisitURemInst(URemInst &i) override;
  bool VisitMulInst(MulInst &i) override;

  bool VisitCmpInst(CmpInst &i) override;

  bool VisitSelectInst(SelectInst &i) override;
  bool VisitOUMulInst(OUMulInst &i) override;
  bool VisitOUAddInst(OUAddInst &i) override;

  bool VisitGetInst(GetInst &i) override;

  bool VisitX86_OutInst(X86_OutInst &i) override;
  bool VisitX86_LgdtInst(X86_LgdtInst &i) override;
  bool VisitX86_LidtInst(X86_LidtInst &i) override;
  bool VisitX86_LtrInst(X86_LtrInst &i) override;
  bool VisitX86_WrMsrInst(X86_WrMsrInst &i) override;
  bool VisitX86_RdTscInst(X86_RdTscInst &i) override;

private:
  /// Return the current frame.
  ID<SymbolicFrame> GetFrame();
  /// Return the instruction to pin values to.
  SymbolicValue::Origin GetOrigin();

private:
  /// Reference to the heap mapping.
  SymbolicHeap &heap_;
  /// Context - information about data flow in the current function.
  SymbolicFrame &frame_;
  /// Graph to approximate symbols referenced by functions.
  ReferenceGraph &refs_;
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
  /// Instruction evaluated.
  Inst &inst_;
};
