// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"




// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSllInst(SllInst &i)
{
  class Visitor final : public BinaryVisitor<SllInst> {
  public:
    Visitor(SymbolicContext &ctx, const SllInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l.shl(r.getSExtValue()));
    }

    SymbolicValue Visit(const APInt &, LowerBoundedInteger) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(LowerBoundedInteger l, const APInt &r) override
    {
      auto newBound = l.Bound.shl(r.getSExtValue());
      if (newBound.isNonNegative()) {
        return SymbolicValue::LowerBoundedInteger(newBound);
      } else {
        return SymbolicValue::Scalar();
      }
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      return SymbolicValue::Value(l.Ptr->Decay());
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      return SymbolicValue::Value(l.Ptr->Decay());
    }

    SymbolicValue Visit(Nullable l, const APInt &r) override
    {
      return SymbolicValue::Value(l.Ptr->Decay());
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSrlInst(SrlInst &i)
{
  class Visitor final : public BinaryVisitor<SrlInst> {
  public:
    Visitor(SymbolicContext &ctx, const SrlInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(LowerBoundedInteger l, const APInt &r) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      return SymbolicValue::Pointer(l.Ptr->Decay());
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      return SymbolicValue::Value(l.Ptr->Decay());
    }

    SymbolicValue Visit(Nullable l, const APInt &r) override
    {
      return SymbolicValue::Value(l.Ptr->Decay());
    }

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l.lshr(r.getZExtValue()));
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSraInst(SraInst &i)
{
  class Visitor final : public BinaryVisitor<SraInst> {
  public:
    Visitor(SymbolicContext &ctx, const SraInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l.ashr(r.getZExtValue()));
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      return SymbolicValue::Value(l.Ptr->Decay());
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}
