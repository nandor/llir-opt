// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/extern.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"



/**
 * Visitor to evaluate the comparison instruction.
 */
class CmpEvalVisitor final : public BinaryVisitor<CmpInst> {
public:
  CmpEvalVisitor(SymbolicContext &ctx, const CmpInst &i) : BinaryVisitor(ctx, i) {}

  SymbolicValue Visit(const APInt &l, const APInt &r) override
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
  }

  SymbolicValue Visit(const APFloat &l, const APFloat &r) override
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

  SymbolicValue Visit(Pointer l, Scalar r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Value l, Scalar r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Pointer l, Pointer r) override
  {
    auto lbegin = l.Ptr.begin();
    auto rbegin = r.Ptr.begin();

    if (!l.Ptr.empty() && std::next(lbegin) == l.Ptr.end()) {
      if (!r.Ptr.empty() && std::next(rbegin) == r.Ptr.end()) {
        if (lbegin->IsPrecise() && rbegin->IsPrecise()) {
          bool eq = *lbegin == *rbegin;
          switch (inst_.GetCC()) {
            case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(eq);
            case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(!eq);
            case Cond::LT: case Cond::OLT: return SymbolicValue::Scalar();
            case Cond::ULT:                return SymbolicValue::Scalar();
            case Cond::LE: case Cond::OLE: return SymbolicValue::Scalar();
            case Cond::ULE:                return SymbolicValue::Scalar();
            case Cond::GT: case Cond::OGT: return SymbolicValue::Scalar();
            case Cond::UGT:                return SymbolicValue::Scalar();
            case Cond::GE: case Cond::OGE: return SymbolicValue::Scalar();
            case Cond::UGE:                return SymbolicValue::Scalar();
            case Cond::O:
            case Cond::UO: llvm_unreachable("invalid integer code");
          }
        }
      }
    }
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Pointer l, Nullable r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(const APInt &l, LowerBoundedInteger r) override
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
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(LowerBoundedInteger l, const APInt &r) override
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
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(LowerBoundedInteger, Value) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Pointer l, const APInt &r) override
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
    } else {
      return SymbolicValue::Scalar();
    }
  }

  SymbolicValue Visit(const APInt &l, Pointer r) override
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
    } else {
      return SymbolicValue::Scalar();
    }
  }

  SymbolicValue Visit(Pointer l, Value r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Value l, const APInt &r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Nullable l, const APInt &r) override
  {
    if (r.isNullValue()) {
      return SymbolicValue::Scalar();
    } else {
      llvm_unreachable("not implemented");
    }
  }

  SymbolicValue Visit(const APInt &l, Value r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Value l, Value r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Value l, Pointer r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Value l, Nullable r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Nullable l, Value r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Nullable l, Nullable r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Undefined, Value) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(Scalar l, Pointer r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Visit(LowerBoundedInteger l, LowerBoundedInteger r) override
  {
    return SymbolicValue::Scalar();
  }

  SymbolicValue Flag(bool value)
  {
    switch (auto ty = inst_.GetType()) {
      case Type::I8:
      case Type::I16:
      case Type::I32:
      case Type::I64:
      case Type::I128: {
        return SymbolicValue::Integer(APInt(GetSize(ty) * 8, value, true));
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

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitCmpInst(CmpInst &i)
{
  return ctx_.Set(i, CmpEvalVisitor(ctx_, i).Dispatch());
}
