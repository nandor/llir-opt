// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/extern.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"



// -----------------------------------------------------------------------------
bool SymbolicEval::VisitCmpInst(CmpInst &i)
{
  class Visitor final : public BinaryVisitor<CmpInst> {
  public:
    Visitor(SymbolicEval &e, CmpInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, Scalar) override { return SetScalar(); }

    bool Visit(Undefined l, const APInt &r) override { return SetUndefined(); }

    bool Visit(Scalar l, const APFloat &r) override { return SetScalar(); }

    bool Visit(const APInt &l, const APInt &r) override
    {
      switch (inst_.GetCC()) {
        case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(l == r);
        case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(l != r);
        case Cond::LT: case Cond::OLT: return Flag(l.slt(r));
        case Cond::ULT:                return Flag(l.ult(r));
        case Cond::GT: case Cond::OGT: return Flag(l.sgt(r));
        case Cond::UGT:                return Flag(l.ugt(r));
        case Cond::LE: case Cond::OLE: return Flag(l.sle(r));
        case Cond::ULE:                return Flag(l.ule(r));
        case Cond::GE: case Cond::OGE: return Flag(l.sge(r));
        case Cond::UGE:                return Flag(l.uge(r));
        case Cond::O:
        case Cond::UO: llvm_unreachable("invalid integer code");
      }
      llvm_unreachable("invalid condition code");
    }

    bool Visit(const APFloat &l, const APFloat &r) override
    {
      switch (inst_.GetCC()) {
        case Cond::NE: case Cond::ONE: case Cond::UNE: {
          if (l == r) {
            return Flag(false);
          }
          llvm_unreachable("not implemented");
        }
        default: {
          llvm_unreachable("not implemented");
        }
      }
    }

    bool Visit(Pointer l, Scalar r) override
    {
      return SetScalar();
    }

    bool Visit(Value l, Scalar r) override
    {
      return SetScalar();
    }

    bool Visit(Pointer l, Pointer r) override
    {
      auto lbegin = l.Ptr->begin();
      auto rbegin = r.Ptr->begin();

      if (!l.Ptr->empty() && std::next(lbegin) == l.Ptr->end()) {
        if (!r.Ptr->empty() && std::next(rbegin) == r.Ptr->end()) {
          if (lbegin->IsPrecise() && rbegin->IsPrecise()) {
            bool eq = *lbegin == *rbegin;
            switch (inst_.GetCC()) {
              case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(eq);
              case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(!eq);
              case Cond::LT: case Cond::OLT: return SetScalar();
              case Cond::ULT:                return SetScalar();
              case Cond::LE: case Cond::OLE: return SetScalar();
              case Cond::ULE:                return SetScalar();
              case Cond::GT: case Cond::OGT: return SetScalar();
              case Cond::UGT:                return SetScalar();
              case Cond::GE: case Cond::OGE: return SetScalar();
              case Cond::UGE:                return SetScalar();
              case Cond::O:
              case Cond::UO: llvm_unreachable("invalid integer code");
            }
          }
        }
      }
      return SetScalar();
    }

    bool Visit(Pointer l, Nullable r) override
    {
      return SetScalar();
    }

    bool Visit(Nullable l, Pointer r) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &l, LowerBoundedInteger r) override
    {
      if (l.isNonNegative()) {
        switch (inst_.GetCC()) {
          case Cond::EQ: {
            if (l.ult(r.Bound)) {
              return Flag(false);
            }
            break;
          }
          case Cond::NE: {
            if (l.ult(r.Bound)) {
              return Flag(true);
            }
            break;
          }
          case Cond::GE: {
            if (l.ult(r.Bound)) {
              return Flag(false);
            }
            break;
          }
          case Cond::LT: case Cond::ULT: case Cond::OLT: {
            if (l.ult(r.Bound)) {
              return Flag(true);
            }
            break;
          }
          default: {
            llvm_unreachable("not implemented");
          }
        }
      }
      return SetScalar();
    }

    bool Visit(LowerBoundedInteger l, const APInt &r) override
    {
      if (r.isNonNegative()) {
        switch (inst_.GetCC()) {
          case Cond::EQ: {
            if (r.ult(l.Bound)) {
              return Flag(false);
            }
            break;
          }
          case Cond::NE: {
            if (r.ult(l.Bound)) {
              return Flag(true);
            }
            break;
          }
          case Cond::LT: case Cond::ULT: case Cond::OLT: {
            if (l.Bound.ugt(r)) {
              return Flag(false);
            }
            break;
          }
          default: {
            llvm_unreachable("not implemented");
          }
        }
      }
      return SetScalar();
    }

    bool Visit(Mask l, const APInt &r) override
    {
      auto mask = l.Known & (l.Value ^ r);
      if (mask.isNullValue()) {
        return SetScalar();
      }
      switch (inst_.GetCC()) {
        case Cond::EQ: return Flag(false);
        case Cond::NE: return Flag(true);
        default: {
          llvm_unreachable("not implemented");
        }
      }
    }

    bool Visit(LowerBoundedInteger, Value) override
    {
      return SetScalar();
    }

    bool Visit(Value, Undefined) override
    {
      return SetUndefined();
    }

    bool Visit(Value, LowerBoundedInteger) override
    {
      return SetScalar();
    }

    bool Visit(Pointer l, const APInt &r) override
    {
      if (r.isNullValue()) {
        switch (inst_.GetCC()) {
          case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(false);
          case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(true);
          case Cond::LT: case Cond::OLT: llvm_unreachable("not implemented");
          case Cond::ULT:                llvm_unreachable("not implemented");
          case Cond::GT: case Cond::OGT: llvm_unreachable("not implemented");
          case Cond::UGT:                llvm_unreachable("not implemented");
          case Cond::LE: case Cond::OLE: llvm_unreachable("not implemented");
          case Cond::ULE:                llvm_unreachable("not implemented");
          case Cond::GE: case Cond::OGE: llvm_unreachable("not implemented");
          case Cond::UGE:                llvm_unreachable("not implemented");
          case Cond::O:
          case Cond::UO: llvm_unreachable("invalid integer code");
        }
        llvm_unreachable("invalid condition code");
      } else {
        return SetScalar();
      }
    }

    bool Visit(const APInt &l, Pointer r) override
    {
      if (l.isNullValue()) {
        switch (inst_.GetCC()) {
          case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(false);
          case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(true);
          case Cond::LT: case Cond::OLT: case Cond::ULT: return Flag(true);
          case Cond::LE: case Cond::OLE: case Cond::ULE: return Flag(true);
          case Cond::GT: case Cond::OGT: llvm_unreachable("not implemented");
          case Cond::UGT:                llvm_unreachable("not implemented");
          case Cond::GE: case Cond::OGE: llvm_unreachable("not implemented");
          case Cond::UGE:                llvm_unreachable("not implemented");
          case Cond::O:
          case Cond::UO: llvm_unreachable("invalid integer code");
        }
        llvm_unreachable("invalid condition code");
      } else {
        return SetScalar();
      }
    }

    bool Visit(Pointer l, Value r) override
    {
      return SetScalar();
    }

    bool Visit(Value l, const APInt &r) override
    {
      return SetScalar();
    }

    bool Visit(Nullable l, const APInt &r) override
    {
      if (r.isNullValue()) {
        return SetScalar();
      } else {
        llvm_unreachable("not implemented");
      }
    }

    bool Visit(const APInt &l, Value r) override
    {
      return SetScalar();
    }

    bool Visit(Value l, Value r) override
    {
      return SetScalar();
    }

    bool Visit(Value l, Pointer r) override
    {
      return SetScalar();
    }

    bool Visit(Value l, Nullable r) override
    {
      return SetScalar();
    }

    bool Visit(Nullable l, Value r) override
    {
      return SetScalar();
    }

    bool Visit(Nullable l, Nullable r) override
    {
      return SetScalar();
    }

    bool Visit(Undefined, Value) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, Pointer) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, Value) override
    {
      return SetScalar();
    }

    bool Visit(LowerBoundedInteger l, LowerBoundedInteger r) override
    {
      return SetScalar();
    }

    bool Flag(bool value)
    {
      switch (auto ty = inst_.GetType()) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::I128: {
          return SetInteger(APInt(GetBitWidth(ty), value, true));
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::V64:
        case Type::F128: {
          llvm_unreachable("invalid comparison");
        }
      }
      llvm_unreachable("invalid type");
    }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSelectInst(SelectInst &i)
{
  auto condVal = ctx_.Find(i.GetCond());
  auto trueVal = ctx_.Find(i.GetTrue());
  auto falseVal = ctx_.Find(i.GetFalse());
  if (condVal.IsTrue()) {
    return ctx_.Set(i, trueVal);
  }
  if (condVal.IsFalse()) {
    return ctx_.Set(i, falseVal);
  }
  return ctx_.Set(i, trueVal.LUB(falseVal).Pin(i.GetSubValue(0), GetFrame()));
}
