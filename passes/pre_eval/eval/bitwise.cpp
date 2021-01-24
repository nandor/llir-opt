// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/atom.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"



// -----------------------------------------------------------------------------
bool SymbolicEval::VisitAndInst(AndInst &i)
{
  class Visitor final : public BinaryVisitor<AndInst> {
  public:
    Visitor(SymbolicEval &e, AndInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, Value r) override
    {
      return SetValue(r.Ptr->Decay());
    }

    bool Visit(const APInt &l, Scalar r) override
    {
      return Visit(r, l);
    }

    bool Visit(const APInt &lhs, const APInt &rhs) override
    {
      return SetInteger(lhs & rhs);
    }

    bool Visit(Mask l, const APInt &r) override
    {
      return SetMask(l.Known, r & l.Value & l.Known);
    }

    bool Visit(Nullable l, const APInt &r) override
    {
      return PointerAnd(l.Ptr, r, true);
    }

    bool Visit(Pointer l, const APInt &r) override
    {
      return PointerAnd(l.Ptr, r, false);
    }

    bool Visit(Value l, const APInt &rhs) override
    {
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(Value l, Scalar) override
    {
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(Value l, Pointer r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Value l, Value r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

  private:
    bool
    PointerAnd(const SymbolicPointer::Ref &ptr, const APInt &r, bool nullable)
    {
      if (r.isNullValue()) {
        return SetPointer(ptr);
      }

      auto begin = ptr->begin();
      if (!ptr->empty() && std::next(begin) == ptr->end()) {
        switch (begin->GetKind()) {
          case SymbolicAddress::Kind::OBJECT: {
            auto &a = begin->AsObject();
            auto &object = ctx_.GetObject(a.Object);
            auto align = object.GetAlignment();
            if (r.getBitWidth() <= 64) {
              if (align.value() % (r.getSExtValue() + 1) == 0) {
                auto bits = a.Offset & r.getSExtValue();
                if (nullable && bits != 0) {
                  return SetScalar();
                }
                return SetInteger(APInt(64, bits, true));
              }
            }
            return SetValue(ptr->Decay());
          }
          case SymbolicAddress::Kind::EXTERN: {
            return SetValue(ptr->Decay());
          }
          case SymbolicAddress::Kind::FUNC:
          case SymbolicAddress::Kind::BLOCK:
          case SymbolicAddress::Kind::STACK: {
            return SetValue(ptr->Decay());
          }
          case SymbolicAddress::Kind::OBJECT_RANGE:
          case SymbolicAddress::Kind::EXTERN_RANGE: {
            return SetValue(ptr->Decay());
          }
        }
        llvm_unreachable("invalid address kind");
      }

      if (r.getBitWidth() <= 64) {
        if (0 <= r.getSExtValue() && r.getSExtValue() <= 8) {
          return SetScalar();
        }
      }

      return SetValue(ptr->Decay());
    }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitOrInst(OrInst &i)
{
  class Visitor final : public BinaryVisitor<OrInst> {
  public:
    Visitor(SymbolicEval &e, OrInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &lhs, const APInt &rhs) override
    {
      return SetInteger(lhs | rhs);
    }

    bool Visit(Pointer l, const APInt &rhs) override
    {
      if (rhs.isNullValue()) {
        return SetPointer(l.Ptr);
      }
      return SetPointer(l.Ptr->Decay());
    }

    bool Visit(Value l, const APInt &rhs) override
    {
      if (rhs.isNullValue()) {
        return SetValue(l.Ptr);
      }
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(const APInt &l, Value r) override
    {
      if (l.isNullValue()) {
        return SetValue(r.Ptr);
      }
      return SetValue(r.Ptr->Decay());
    }

    bool Visit(Pointer l, Scalar) override
    {
      return SetPointer(l.Ptr->Decay());
    }

    bool Visit(Pointer l, Pointer r) override
    {
      return SetPointer(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Value l, Value r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }

    bool Visit(Value l, Scalar) override
    {
      return SetValue(l.Ptr->Decay());
    }

    bool Visit(Pointer l, Value r) override
    {
      return SetValue(l.Ptr->LUB(r.Ptr));
    }
  };
  return Visitor(*this, i).Evaluate();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitXorInst(XorInst &i)
{
  class Visitor final : public BinaryVisitor<XorInst> {
  public:
    Visitor(SymbolicEval &e, XorInst &i) : BinaryVisitor(e, i) {}

    bool Visit(Scalar l, const APInt &) override
    {
      return SetScalar();
    }

    bool Visit(Scalar, Value r) override
    {
      return SetValue(r.Ptr->Decay());
    }

    bool Visit(LowerBoundedInteger, Scalar) override
    {
      return SetScalar();
    }

    bool Visit(const APInt &l, const APInt &r) override
    {
      return SetInteger(l ^ r);
    }

    bool Visit(const APInt &l, Value r) override
    {
      if (l.isNullValue()) {
        return ctx_.Set(inst_, rhs_);
      }
      return SetValue(r.Ptr->Decay());
    }

    bool Visit(Value l, const APInt &r) override { return Visit(r, l); }

    bool Visit(Value, Value) override
    {
      return SetScalar();
    }

    bool Visit(Value, Nullable) override
    {
      return SetScalar();
    }

    bool Visit(Value, Pointer) override
    {
      return SetScalar();
    }

    bool Visit(Nullable, Nullable) override
    {
      return SetScalar();
    }

    bool Visit(Value l, Scalar r) override { return Visit(r, l); }
  };
  return Visitor(*this, i).Evaluate();
}
