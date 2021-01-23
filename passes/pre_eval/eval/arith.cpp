// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"


// -----------------------------------------------------------------------------
static SymbolicPointer::Ref
OffsetPointer(const SymbolicPointer::Ref &ptr, const APInt &off)
{
  if (off.getBitWidth() <= 64) {
    return ptr->Offset(off.getSExtValue());
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitAddInst(AddInst &i)
{
  class Visitor final : public BinaryVisitor<AddInst> {
  public:
    Visitor(SymbolicEval &e, AddInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, const APFloat &) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, Pointer r) override
    {
      return SetPointer(r.Ptr->Decay());
    }

    bool Visit(Scalar, Value r) override
    {
      return SetValue(r.Ptr->Decay());
    }

    bool Visit(Scalar, Nullable r) override
    {
      return SetValue(r.Ptr->Decay());
    }

    bool Visit(LowerBoundedInteger l, const APInt &r) override
    {
      assert(l.Bound.getBitWidth() == r.getBitWidth() && "invalid operands");
      if (l.Bound.getBitWidth() <= 64) {
        auto newBound = l.Bound + r;
        if (newBound.isNonNegative()) {
          return SetLowerBounded(newBound);
        } else {
          return SetScalar();
        }
      } else {
        return SetScalar();
      }
    }

    bool Visit(LowerBoundedInteger l, LowerBoundedInteger r) override
    {
      auto newBound = l.Bound + r.Bound;
      if (newBound.isNonNegative()) {
        return SetLowerBounded(newBound);
      } else {
        return SetScalar();
      }
    }

    bool Visit(LowerBoundedInteger, Pointer r) override
    {
      return SetPointer(r.Ptr->Decay());
    }

    bool Visit(LowerBoundedInteger, Value r) override
    {
      return SetValue(r.Ptr->Decay());
    }

    bool Visit(const APInt &l, Scalar r) override { return Visit(r, l); }

    bool Visit(const APInt &l, LowerBoundedInteger r) override
    {
      return Visit(r, l);
    }

    bool Visit(const APInt &l, Pointer r) override
    {
      return SetPointer(OffsetPointer(r.Ptr, l));
    }

    bool Visit(const APInt &l, Value r) override
    {
      return SetValue(OffsetPointer(r.Ptr, l));
    }

    bool Visit(const APInt &l, Nullable r) override
    {
      return SetValue(OffsetPointer(r.Ptr, l));
    }

    bool Visit(const APInt &l, const APInt &r) override
    {
      return SetInteger(l + r);
    }

    bool Visit(Pointer l, Scalar r) override { return Visit(r, l); }

    bool Visit(Pointer l, const APInt &r) override
    {
      return Visit(r, l);
    }

    bool Visit(Pointer l, Pointer r) override
    {
      return SetPointer(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Pointer l, Value r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Pointer l, Nullable r) override
    {
      return SetNullable(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Value l, LowerBoundedInteger r) override { return Visit(r, l); }

    bool Visit(Value l, const APInt &r) override
    {
      return SetValue(OffsetPointer(l.Ptr, r));
    }

    bool Visit(Value l, Scalar r) override { return Visit(r, l); }

    bool Visit(Value l, Value r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Value l, Pointer r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Value l, Nullable r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Nullable l, const APInt &r) override { return Visit(r, l); }

    bool Visit(Nullable l, Scalar r) override { return Visit(r, l); }

    bool Visit(Nullable l, Value r) override { return Visit(r, l); }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSubInst(SubInst &i)
{
  class Visitor final : public BinaryVisitor<SubInst> {
  public:
    Visitor(SymbolicEval &e, SubInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &l, const APInt &r) override
    {
      return SetInteger(l - r);
    }

    bool Visit(const APInt &l, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &l, Value) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(Scalar l, Pointer r) override
    {
      return SetScalar();
    }

    bool Visit(Scalar l, Value r) override
    {
      return SetScalar();
    }

    bool Visit(Pointer l, const APInt &r) override
    {
      if (r.getBitWidth() <= 64) {
        return SetPointer(l.Ptr->Offset(-r.getSExtValue()));
      } else {
        llvm_unreachable("not implemented");
      }
    }

    bool Visit(Pointer l, Scalar) override
    {
      return SetPointer(l.Ptr->Decay());
    }

    bool Visit(Value l, const APInt &r) override
    {
      if (r.getBitWidth() <= 64) {
        return SetValue(l.Ptr->Offset(-r.getSExtValue()));
      } else {
        llvm_unreachable("not implemented");
      }
    }

    bool Visit(Value l, Value r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Value l, Pointer r) override
    {
      return SetScalar();
    }

    bool Visit(Value l, Scalar) override
    {
      return SetPointer(l.Ptr->Decay());
    }

    bool Visit(Pointer l, Value r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Pointer l, Pointer r) override
    {
      return SetPointerDiff(l.Ptr, r.Ptr);
    }

    bool Visit(Pointer l, Nullable r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Value l, Nullable r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Nullable l, Nullable r) override
    {
      return SetPointerDiff(l.Ptr, r.Ptr);
    }

    bool Visit(Nullable l, Value r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Nullable l, const APInt &r) override
    {
      return SetValue(OffsetPointer(l.Ptr, -r));
    }

  private:
    bool SetPointerDiff(
        const SymbolicPointer::Ref &lptr,
        const SymbolicPointer::Ref &rptr)
    {
      auto lbegin = lptr->begin();
      auto rbegin = rptr->begin();

      if (!lptr->empty() && std::next(lbegin) == lptr->end()) {
        if (!rptr->empty() && std::next(rbegin) == rptr->end()) {
          switch (lbegin->GetKind()) {
            case SymbolicAddress::Kind::OBJECT: {
              auto &lg = lbegin->AsObject();
              if (auto *rg = rbegin->ToObject()) {
                if (lg.Object == rg->Object) {
                  int64_t diff = lg.Offset - rg->Offset;
                  return SetInteger(APInt(64, diff, true));
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
                  // return SetScalar();
                  llvm_unreachable("not implemented");
                } else {
                  llvm_unreachable("not implemented");
                }
              }
              return SetValue(lptr->LUB(rptr));
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

      return SetValue(lptr->LUB(rptr));
    }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitUDivInst(UDivInst &i)
{
  class Visitor final : public BinaryVisitor<UDivInst> {
  public:
    Visitor(SymbolicEval &e, UDivInst &i) : BinaryVisitor(e, i) {}

    bool Visit(const APInt &, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &l, const APInt &r) override
    {
      if (r.isNullValue()) {
        llvm_unreachable("not implemented");
      } else {
        return SetInteger(l.udiv(r));
      }
    }

    bool Visit(Value l, const APInt &) override
    {
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(Pointer l, const APInt &) override
    {
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(LowerBoundedInteger, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(LowerBoundedInteger, const APInt &) override
    {
      return SetScalar();
    }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitURemInst(URemInst &i)
{
  class Visitor final : public BinaryVisitor<URemInst> {
  public:
    Visitor(SymbolicEval &e, URemInst &i) : BinaryVisitor(e, i) {}

    bool Visit(const APInt &l, const APInt &r) override
    {
      return SetInteger(l.urem(r));
    }

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSDivInst(SDivInst &i)
{
  class Visitor final : public BinaryVisitor<SDivInst> {
  public:
    Visitor(SymbolicEval &e, SDivInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &l, const APInt &r) override
    {
      if (r.isNullValue()) {
        llvm_unreachable("not implemented");
      } else {
        return SetInteger(l.sdiv(r));
      }
    }

    bool Visit(Value l, const APInt &) override
    {
      return SetValue(l.Ptr->Decay());
    }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMulInst(MulInst &i)
{
  class Visitor final : public BinaryVisitor<MulInst> {
  public:
    Visitor(SymbolicEval &e, MulInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &l, const APInt &r) override
    {
      return SetInteger(l * r);
    }

    bool Visit(const APInt &, Pointer) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &, Value) override
    {
      return SetScalar();
    }

    bool Visit(Pointer l, const APInt &r) override { return Visit(r, l); }
    bool Visit(Value l, const APInt &r) override { return Visit(r, l); }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitOUMulInst(OUMulInst &i)
{
  class Visitor final : public BinaryVisitor<OUMulInst> {
  public:
    Visitor(SymbolicEval &e, OUMulInst &i) : BinaryVisitor(e, i) {}

    bool Visit(const APInt &l, const APInt &r) override
    {
      Type ty = inst_.GetType();
      bool overflow = false;
      (void) l.umul_ov(r, overflow);
      return SetInteger(APInt(GetBitWidth(ty), overflow, true));
    }

    bool Visit(const APInt &, Value) override
    {
      return SetScalar();
    }

    bool Visit(Value l, const APInt &r) override { return Visit(r, l); }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitOUAddInst(OUAddInst &i)
{
  class Visitor final : public BinaryVisitor<OUAddInst> {
  public:
    Visitor(SymbolicEval &e, OUAddInst &i) : BinaryVisitor(e, i) {}

    bool Visit(const APInt &l, const APInt &r) override
    {
      Type ty = inst_.GetType();
      bool overflow = false;
      (void) l.uadd_ov(r, overflow);
      return SetInteger(APInt(GetBitWidth(ty), overflow, true));
    }
  };
  return Visitor(*this, i).Evaluate();
}
