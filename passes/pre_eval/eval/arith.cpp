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

    SymbolicValue Visit(const APInt &l, LowerBoundedInteger r) override
    {
      return Visit(r, l);
    }

    SymbolicValue Visit(LowerBoundedInteger l, Pointer r) override
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

    SymbolicValue Visit(Scalar, const APInt &) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Scalar l, Pointer r) override
    {
      return SymbolicValue::Pointer(r.Ptr.Decay());
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      if (r.getBitWidth() <= 64) {
        return SymbolicValue::Pointer(l.Ptr.Offset(-r.getSExtValue()));
      } else {
        llvm_unreachable("not implemented");
      }
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

    SymbolicValue Visit(Pointer l, Pointer r) override
    {
      auto lub = [&]
      {
        SymbolicPointer v(l.Ptr);
        v.LUB(r.Ptr);
        return SymbolicValue::Value(v);
      };

      auto lbegin = l.Ptr.begin();
      auto rbegin = r.Ptr.begin();

      if (!l.Ptr.empty() && std::next(lbegin) == l.Ptr.end()) {
        if (!r.Ptr.empty() && std::next(rbegin) == r.Ptr.end()) {
          switch (lbegin->GetKind()) {
            case SymbolicAddress::Kind::GLOBAL: {
              auto &lg = lbegin->AsGlobal();
              if (auto *rg = rbegin->ToGlobal()) {
                if (lg.Symbol == rg->Symbol) {
                  int64_t diff = lg.Offset - rg->Offset;
                  return SymbolicValue::Integer(APInt(64, diff, true));
                } else {
                  llvm_unreachable("not implemented");
                }
              }
              llvm_unreachable("not implemented");
            }
            case SymbolicAddress::Kind::GLOBAL_RANGE: {
              auto &lrange = lbegin->AsGlobalRange();
              if (auto *rg = rbegin->ToGlobal()) {
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

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      return SymbolicValue::Scalar();
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}
