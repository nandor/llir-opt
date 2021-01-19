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
    Visitor(SymbolicContext &ctx, const AndInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &lhs, const APInt &rhs) override
    {
      return SymbolicValue::Integer(lhs & rhs);
    }

    SymbolicValue Visit(Nullable l, const APInt &r) override
    {
      return PointerAnd(l.Ptr, r, true);
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      return PointerAnd(l.Ptr, r, false);
    }

    SymbolicValue Visit(Value l, const APInt &rhs) override
    {
      return SymbolicValue::Value(l.Ptr->Decay());
    }

    SymbolicValue Visit(Value l, Scalar) override
    {
      return SymbolicValue::Value(l.Ptr->Decay());
    }

    SymbolicValue Visit(Value l, Pointer r) override
    {
      return SymbolicValue::Value(l.Ptr->LUB(r.Ptr));
    }

    SymbolicValue Visit(Value l, Value r) override
    {
      return SymbolicValue::Value(l.Ptr->LUB(r.Ptr));
    }

  private:
    SymbolicValue
    PointerAnd(const SymbolicPointer::Ref &ptr, const APInt &r, bool nullable)
    {
      if (r.isNullValue()) {
        return SymbolicValue::Pointer(ptr);
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
                  return SymbolicValue::Scalar();
                }
                return SymbolicValue::Integer(APInt(64, bits, true));
              }
            }
            return SymbolicValue::Value(ptr->Decay());
          }
          case SymbolicAddress::Kind::EXTERN: {
            return SymbolicValue::Value(ptr->Decay());
          }
          case SymbolicAddress::Kind::FUNC:
          case SymbolicAddress::Kind::BLOCK:
          case SymbolicAddress::Kind::STACK: {
            return SymbolicValue::Value(ptr->Decay());
          }
          case SymbolicAddress::Kind::OBJECT_RANGE:
          case SymbolicAddress::Kind::EXTERN_RANGE: {
            return SymbolicValue::Value(ptr->Decay());
          }
        }
        llvm_unreachable("invalid address kind");
      }

      if (r.getBitWidth() <= 64) {
        if (0 <= r.getSExtValue() && r.getSExtValue() <= 8) {
          return SymbolicValue::Scalar();
        }
      }

      return SymbolicValue::Value(ptr->Decay());
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitOrInst(OrInst &i)
{
  class Visitor final : public BinaryVisitor<OrInst> {
  public:
    Visitor(SymbolicContext &ctx, const OrInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &lhs, const APInt &rhs) override
    {
      return SymbolicValue::Integer(lhs | rhs);
    }

    SymbolicValue Visit(Pointer l, const APInt &rhs) override
    {
      if (rhs.isNullValue()) {
        return SymbolicValue::Pointer(l.Ptr);
      }
      return SymbolicValue::Pointer(l.Ptr->Decay());
    }

    SymbolicValue Visit(Value l, const APInt &rhs) override
    {
      if (rhs.isNullValue()) {
        return SymbolicValue::Value(l.Ptr);
      }
      return SymbolicValue::Value(l.Ptr->Decay());
    }

    SymbolicValue Visit(const APInt &l, Value r) override
    {
      if (l.isNullValue()) {
        return SymbolicValue::Value(r.Ptr);
      }
      return SymbolicValue::Value(r.Ptr->Decay());
    }

    SymbolicValue Visit(Pointer l, Scalar) override
    {
      return SymbolicValue::Pointer(l.Ptr->Decay());
    }

    SymbolicValue Visit(Pointer l, Pointer r) override
    {
      return SymbolicValue::Pointer(l.Ptr->LUB(r.Ptr));
    }

    SymbolicValue Visit(Value l, Value r) override
    {
      return SymbolicValue::Value(l.Ptr->LUB(r.Ptr));
    }

    SymbolicValue Visit(Value l, Scalar) override
    {
      return SymbolicValue::Value(l.Ptr->Decay());
    }

    SymbolicValue Visit(Pointer l, Value r) override
    {
      return SymbolicValue::Value(l.Ptr->LUB(r.Ptr));
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitXorInst(XorInst &i)
{
  class Visitor final : public BinaryVisitor<XorInst> {
  public:
    Visitor(SymbolicContext &ctx, const XorInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &lhs, const APInt &rhs) override
    {
      return SymbolicValue::Integer(lhs ^ rhs);
    }

    SymbolicValue Visit(const APInt &l, Value r) override
    {
      if (l.isNullValue()) {
        return rhs_;
      }
      llvm_unreachable("not implemented");
    }

    SymbolicValue Visit(Scalar l, const APInt &) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Value l, const APInt &rhs) override
    {
      return lhs_;
    }

    SymbolicValue Visit(Nullable, Value) override
    {
       return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Nullable, Nullable) override
    {
       return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Value, Value) override
    {
       return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Value, Pointer) override
    {
       return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Value, Scalar) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Value l, Nullable r) override
    {
      return Visit(r, l);
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}
