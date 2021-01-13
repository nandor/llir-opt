// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"


// -----------------------------------------------------------------------------
static SymbolicValue OffsetPointer(const SymbolicPointer &ptr, const APInt &off)
{
  if (off.getBitWidth() <= 64) {
    return SymbolicValue::Pointer(ptr.Offset(off.getSExtValue()));
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
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Pointer(v);
    }

    SymbolicValue Visit(Pointer l, Value r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

    SymbolicValue Visit(Pointer l, Nullable r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Nullable(v);
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      return OffsetPointer(l.Ptr, r);
    }

    SymbolicValue Visit(Pointer l, LowerBoundedInteger) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      return OffsetPointer(l.Ptr, r);
    }

    SymbolicValue Visit(Value l, Scalar) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, LowerBoundedInteger) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, Value r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

    SymbolicValue Visit(Value l, Pointer r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

    SymbolicValue Visit(Nullable l, const APInt &r) override
    {
      return OffsetPointer(l.Ptr, r);
    }

    SymbolicValue Visit(Nullable l, Scalar) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Nullable l, LowerBoundedInteger) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Nullable l, Value r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Pointer(v);
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
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

    SymbolicValue Visit(Value l, Pointer r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

    SymbolicValue Visit(Value, Scalar) override
    {
      return lhs_;
    }

    SymbolicValue Visit(Pointer l, Value r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

    SymbolicValue Visit(Pointer l, Pointer r) override
    {
      return PointerDiff(l.Ptr, r.Ptr);
    }

    SymbolicValue Visit(Pointer l, Nullable r) override
    {
      return PointerDiff(l.Ptr, r.Ptr);
    }

    SymbolicValue Visit(Value l, Nullable r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

    SymbolicValue Visit(Nullable l, Nullable r) override
    {
      return PointerDiff(l.Ptr, r.Ptr);
    }

    SymbolicValue Visit(Nullable l, Value r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

    SymbolicValue Visit(Nullable l, const APInt &r) override
    {
      return OffsetPointer(l.Ptr, -r);
    }

  private:
    SymbolicValue PointerDiff(
        const SymbolicPointer &lptr,
        const SymbolicPointer &rptr)
    {
      auto lub = [&]
      {
        SymbolicPointer v(lptr);
        v.LUB(rptr);
        return SymbolicValue::Value(v);
      };

      auto lbegin = lptr.begin();
      auto rbegin = rptr.begin();

      if (!lptr.empty() && std::next(lbegin) == lptr.end()) {
        if (!rptr.empty() && std::next(rbegin) == rptr.end()) {
          switch (lbegin->GetKind()) {
            case SymbolicAddress::Kind::ATOM: {
              auto &lg = lbegin->AsAtom();
              if (auto *rg = rbegin->ToAtom()) {
                if (lg.Symbol == rg->Symbol) {
                  int64_t diff = lg.Offset - rg->Offset;
                  return SymbolicValue::Integer(APInt(64, diff, true));
                } else {
                  llvm_unreachable("not implemented");
                }
              }
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::ATOM_RANGE: {
              auto &lrange = lbegin->AsAtomRange();
              if (auto *rg = rbegin->ToAtom()) {
                if (lrange.Symbol == rg->Symbol) {
                  return SymbolicValue::Scalar();
                } else {
                  llvm_unreachable("not implemented");
                }
              }
              return lub();
            }
            case SymbolicAddress::Kind::FRAME: {
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::FRAME_RANGE: {
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::HEAP: {
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::HEAP_RANGE: {
              auto &lrange = lbegin->AsHeapRange();
              if (auto *rg = rbegin->ToHeap()) {
                if (lrange.Frame == rg->Frame && lrange.Alloc == rg->Alloc) {
                  return SymbolicValue::Scalar();
                } else {
                  llvm_unreachable("not implemented");
                }
              }
              return lub();
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

      return lub();
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

    SymbolicValue Visit(LowerBoundedInteger l, Scalar) override
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
