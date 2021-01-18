// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"


// -----------------------------------------------------------------------------
static SymbolicPointer OffsetPointer(const SymbolicPointer &ptr, const APInt &off)
{
  if (off.getBitWidth() <= 64) {
    return ptr.Offset(off.getSExtValue());
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitAddInst(AddInst &i)
{
  class Visitor final : public BinaryVisitor<AddInst> {
  public:
    Visitor(SymbolicContext &ctx, const AddInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l + r);
    }

    SymbolicValue Visit(Scalar, const APInt &) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Scalar, Pointer r) override
    {
      return SymbolicValue::Pointer(r.Ptr.Decay());
    }

    SymbolicValue Visit(LowerBoundedInteger l, const APInt &r) override
    {
      assert(l.Bound.getBitWidth() == r.getBitWidth() && "invalid operands");
      if (l.Bound.getBitWidth() <= 64) {
        auto newBound = l.Bound + r;
        if (newBound.isNonNegative()) {
          return SymbolicValue::LowerBoundedInteger(newBound);
        } else {
          return SymbolicValue::Scalar();
        }
      } else {
          return SymbolicValue::Scalar();
      }
    }

    SymbolicValue Visit(LowerBoundedInteger l, LowerBoundedInteger r) override
    {
      auto newBound = l.Bound + r.Bound;
      if (newBound.isNonNegative()) {
        return SymbolicValue::LowerBoundedInteger(newBound);
      } else {
        return SymbolicValue::Scalar();
      }
    }

    SymbolicValue Visit(Pointer l, Scalar) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Pointer l, Pointer r) override
    {
      return SymbolicValue::Pointer(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Pointer l, Value r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Pointer l, Nullable r) override
    {
      return SymbolicValue::Nullable(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      return SymbolicValue::Pointer(OffsetPointer(l.Ptr, r));
    }

    SymbolicValue Visit(Pointer l, LowerBoundedInteger) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      return SymbolicValue::Value(OffsetPointer(l.Ptr, r));
    }

    SymbolicValue Visit(Value l, Scalar) override
    {
      return SymbolicValue::Value(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, LowerBoundedInteger) override
    {
      return SymbolicValue::Value(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, Value r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Value l, Pointer r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Value l, Nullable r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Nullable l, const APInt &r) override
    {
      return SymbolicValue::Value(OffsetPointer(l.Ptr, r));
    }

    SymbolicValue Visit(Nullable l, Scalar) override
    {
      return SymbolicValue::Value(l.Ptr.Decay());
    }

    SymbolicValue Visit(Nullable l, LowerBoundedInteger) override
    {
      return SymbolicValue::Value(l.Ptr.Decay());
    }

    SymbolicValue Visit(Nullable l, Value r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(const APInt &l, Pointer r) override
    {
      return Visit(r, l);
    }

    SymbolicValue Visit(const APInt &l, Value r) override
    {
      return Visit(r, l);
    }

    SymbolicValue Visit(const APInt &l, Nullable r) override
    {
      return Visit(r, l);
    }

    SymbolicValue Visit(const APInt &l, LowerBoundedInteger r) override
    {
      return Visit(r, l);
    }

    SymbolicValue Visit(LowerBoundedInteger l, Pointer r) override
    {
      return Visit(r, l);
    }

    SymbolicValue Visit(Scalar l, Value r) override
    {
      return Visit(r, l);
    }

    SymbolicValue Visit(Scalar l, Nullable r) override
    {
      return SymbolicValue::Value(r.Ptr);
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSubInst(SubInst &i)
{
  class Visitor final : public BinaryVisitor<SubInst> {
  public:
    Visitor(SymbolicContext &ctx, const SubInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l - r);
    }

    SymbolicValue Visit(const APInt &l, Value) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Scalar, const APInt &) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Scalar l, Pointer r) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Scalar l, Value r) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      if (r.getBitWidth() <= 64) {
        return SymbolicValue::Pointer(l.Ptr.Offset(-r.getSExtValue()));
      } else {
        llvm_unreachable("not implemented");
      }
    }

    SymbolicValue Visit(Pointer l, Scalar r) override
    {
      return lhs_;
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      if (r.getBitWidth() <= 64) {
        return SymbolicValue::Value(l.Ptr.Offset(-r.getSExtValue()));
      } else {
        llvm_unreachable("not implemented");
      }
    }

    SymbolicValue Visit(Value l, Value r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Value l, Pointer r) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Value, Scalar) override
    {
      return lhs_;
    }

    SymbolicValue Visit(Pointer l, Value r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Pointer l, Pointer r) override
    {
      return PointerDiff(l.Ptr, r.Ptr);
    }

    SymbolicValue Visit(Pointer l, Nullable r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Value l, Nullable r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Nullable l, Nullable r) override
    {
      return PointerDiff(l.Ptr, r.Ptr);
    }

    SymbolicValue Visit(Nullable l, Value r) override
    {
      return SymbolicValue::Value(l.Ptr.LUB(r.Ptr));
    }

    SymbolicValue Visit(Nullable l, const APInt &r) override
    {
      return SymbolicValue::Value(OffsetPointer(l.Ptr, -r));
    }

  private:
    SymbolicValue PointerDiff(
        const SymbolicPointer &lptr,
        const SymbolicPointer &rptr)
    {
      auto lbegin = lptr.begin();
      auto rbegin = rptr.begin();

      if (!lptr.empty() && std::next(lbegin) == lptr.end()) {
        if (!rptr.empty() && std::next(rbegin) == rptr.end()) {
          switch (lbegin->GetKind()) {
            case SymbolicAddress::Kind::OBJECT: {
              auto &lg = lbegin->AsObject();
              if (auto *rg = rbegin->ToObject()) {
                if (lg.Object == rg->Object) {
                  int64_t diff = lg.Offset - rg->Offset;
                  return SymbolicValue::Integer(APInt(64, diff, true));
                } else {
                  llvm_unreachable("not implemented");
                }
              }
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::OBJECT_RANGE: {
              auto &lrange = lbegin->AsObjectRange();
              if (auto *rg = rbegin->ToObject()) {
                if (lrange.Object == rg->Object) {
                  return SymbolicValue::Scalar();
                } else {
                  llvm_unreachable("not implemented");
                }
              }
              return SymbolicValue::Value(lptr.LUB(rptr));
            }
            case SymbolicAddress::Kind::EXTERN: {
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::EXTERN_RANGE: {
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::FUNC: {
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::BLOCK: {
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::STACK: {
              llvm_unreachable("not implemented");
            }
          }
          llvm_unreachable("invalid address kind");
        }
      }

      return SymbolicValue::Value(lptr.LUB(rptr));
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitUDivInst(UDivInst &i)
{
  class Visitor final : public BinaryVisitor<UDivInst> {
  public:
    Visitor(SymbolicContext &ctx, const UDivInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      if (r.isNullValue()) {
        llvm_unreachable("not implemented");
      } else {
        return SymbolicValue::Integer(l.udiv(r));
      }
    }

    SymbolicValue Visit(Value, const APInt &) override
    {
      return lhs_;
    }

    SymbolicValue Visit(Pointer l, const APInt &) override
    {
      return SymbolicValue::Value(l.Ptr);
    }

    SymbolicValue Visit(LowerBoundedInteger, Scalar) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(LowerBoundedInteger, const APInt &) override
    {
      return SymbolicValue::Scalar();
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitURemInst(URemInst &i)
{
  class Visitor final : public BinaryVisitor<URemInst> {
  public:
    Visitor(SymbolicContext &ctx, const URemInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l.urem(r));
    }

    SymbolicValue Visit(Scalar, const APInt &) override
    {
      return SymbolicValue::Scalar();
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSDivInst(SDivInst &i)
{
  class Visitor final : public BinaryVisitor<SDivInst> {
  public:
    Visitor(SymbolicContext &ctx, const SDivInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      if (r.isNullValue()) {
        llvm_unreachable("not implemented");
      } else {
        return SymbolicValue::Integer(l.sdiv(r));
      }
    }

    SymbolicValue Visit(Value, const APInt &) override
    {
      return lhs_;
    }

  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMulInst(MulInst &i)
{
  class Visitor final : public BinaryVisitor<MulInst> {
  public:
    Visitor(SymbolicContext &ctx, const MulInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l * r);
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(const APInt &, Value) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      return SymbolicValue::Scalar();
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitOUMulInst(OUMulInst &i)
{
  class Visitor final : public BinaryVisitor<OUMulInst> {
  public:
    Visitor(SymbolicContext &ctx, const OUMulInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      Type ty = inst_.GetType();
      bool overflow = false;
      (void) l.umul_ov(r, overflow);
      return SymbolicValue::Integer(APInt(GetBitWidth(ty), overflow, true));
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      return SymbolicValue::Scalar();
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitOUAddInst(OUAddInst &i)
{
  class Visitor final : public BinaryVisitor<OUAddInst> {
  public:
    Visitor(SymbolicContext &ctx, const OUAddInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      Type ty = inst_.GetType();
      bool overflow = false;
      (void) l.uadd_ov(r, overflow);
      return SymbolicValue::Integer(APInt(GetBitWidth(ty), overflow, true));
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}
