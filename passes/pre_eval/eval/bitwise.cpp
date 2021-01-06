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
      return SymbolicValue::Value(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, Scalar) override
    {
      return SymbolicValue::Value(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, Pointer r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

  private:
    SymbolicValue PointerAnd(
        const SymbolicPointer &ptr,
        const APInt &r,
        bool nullable)
    {
      if (r.isNullValue()) {
        return SymbolicValue::Pointer(ptr);
      }

      auto begin = ptr.begin();
      if (!ptr.empty() && std::next(begin) == ptr.end()) {
        switch (begin->GetKind()) {
          case SymbolicAddress::Kind::GLOBAL: {
            auto &g = begin->AsGlobal();
            switch (g.Symbol->GetKind()) {
              case Global::Kind::FUNC:
              case Global::Kind::BLOCK: {
                return lhs_;
              }
              case Global::Kind::EXTERN: {
                return SymbolicValue::Pointer(ptr.Decay());
              }
              case Global::Kind::ATOM: {
                auto *atom = static_cast<Atom *>(g.Symbol);
                if (auto align = atom->GetAlignment()) {
                  if (r.getBitWidth() <= 64) {
                    if (align->value() % (r.getSExtValue() + 1) == 0) {
                      auto bits = g.Offset & r.getSExtValue();
                      if (nullable && bits != 0) {
                        return SymbolicValue::Scalar();
                      }
                      return SymbolicValue::Integer(APInt(64, bits, true));
                    }
                  }
                }
                return SymbolicValue::Value(ptr.Decay());
              }
            }
            llvm_unreachable("invalid global kind");
          }
          case SymbolicAddress::Kind::FRAME: {
            return SymbolicValue::Value(ptr.Decay());
          }
          case SymbolicAddress::Kind::HEAP: {
            auto &h = begin->AsHeap();
            // Heap objects are always 8-byte aligned.
            if (r.getBitWidth() <= 64) {
              if (8 % (r.getSExtValue() + 1) == 0) {
                auto bits = h.Offset & r.getSExtValue();
                if (nullable && bits != 0) {
                  return SymbolicValue::Scalar();
                }
                return SymbolicValue::Integer(APInt(64, bits, true));
              }
            }
            return SymbolicValue::Value(ptr.Decay());
          }
          case SymbolicAddress::Kind::FUNC:
          case SymbolicAddress::Kind::BLOCK:
          case SymbolicAddress::Kind::STACK: {
            return SymbolicValue::Value(ptr.Decay());
          }
          case SymbolicAddress::Kind::GLOBAL_RANGE:
          case SymbolicAddress::Kind::FRAME_RANGE:
          case SymbolicAddress::Kind::HEAP_RANGE: {
            return SymbolicValue::Value(ptr.Decay());
          }
        }
        llvm_unreachable("invalid address kind");
      }

      if (r.getBitWidth() <= 64) {
        if (0 <= r.getSExtValue() && r.getSExtValue() <= 8) {
          return SymbolicValue::Scalar();
        }
      }

      return SymbolicValue::Value(ptr.Decay());
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
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, const APInt &rhs) override
    {
      if (rhs.isNullValue()) {
        return SymbolicValue::Value(l.Ptr);
      }
      return SymbolicValue::Value(l.Ptr.Decay());
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

    SymbolicValue Visit(Value l, Value r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
    }

    SymbolicValue Visit(Value l, Scalar) override
    {
      return SymbolicValue::Value(l.Ptr.Decay());
    }

    SymbolicValue Visit(Pointer l, Value r) override
    {
      SymbolicPointer v(l.Ptr);
      v.LUB(r.Ptr);
      return SymbolicValue::Value(v);
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

    SymbolicValue Visit(Scalar l, const APInt &) override
    {
      return SymbolicValue::Scalar();
    }

    SymbolicValue Visit(Nullable, Value) override
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

    SymbolicValue Visit(Value l, Nullable r) override
    {
      return Visit(r, l);
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}
