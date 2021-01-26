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
    Visitor(SymbolicEval &e, SllInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &l, const APInt &r) override
    {
      return SetInteger(l.shl(r.getSExtValue()));
    }

    bool Visit(const APInt &, LowerBoundedInteger) override
    {
      return SetScalar();
    }

    bool Visit(LowerBoundedInteger l, const APInt &r) override
    {
      auto newBound = l.Bound.shl(r.getSExtValue());
      if (newBound.isNonNegative()) {
        return SetLowerBounded(newBound);
      } else {
        return SetScalar();
      }
    }

    bool Visit(Pointer l, const APInt &r) override
    {
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(Value l, const APInt &r) override
    {
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(Nullable l, const APInt &r) override
    {
      return SetValue(l.Ptr->Decay());
    }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSrlInst(SrlInst &i)
{
  class Visitor final : public BinaryVisitor<SrlInst> {
  public:
    Visitor(SymbolicEval &e, SrlInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(LowerBoundedInteger l, const APInt &r) override
    {
      return SetScalar();
    }

    bool Visit(Pointer l, const APInt &r) override
    {
      return SetPointer(l.Ptr->Decay());
    }

    bool Visit(Value l, const APInt &r) override
    {
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(Nullable l, const APInt &r) override
    {
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(const APInt &l, const APInt &r) override
    {
      return SetInteger(l.lshr(r.getZExtValue()));
    }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSraInst(SraInst &i)
{
  class Visitor final : public BinaryVisitor<SraInst> {
  public:
    Visitor(SymbolicEval &e, SraInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &l, const APInt &r) override
    {
      return SetInteger(l.ashr(r.getZExtValue()));
    }

    bool Visit(Value l, const APInt &r) override
    {
      return SetValue(l.Ptr->Decay());
    }
  };
  return Visitor(*this, i).Evaluate();
}
