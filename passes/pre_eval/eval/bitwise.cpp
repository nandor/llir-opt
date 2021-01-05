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

    SymbolicValue Visit(Pointer l, const APInt &rhs) override
    {
      if (rhs.isNullValue()) {
        return SymbolicValue::Pointer(l.Ptr);
      }
      if (rhs.getBitWidth() <= 64) {
        if (0 <= rhs.getSExtValue() && rhs.getSExtValue() <= 8) {
          return SymbolicValue::Scalar();
        }
      }

      auto begin = l.Ptr.begin();
      if (!l.Ptr.empty() && std::next(begin) == l.Ptr.end()) {
        if (auto *g = begin->ToGlobal()) {
          switch (g->Symbol->GetKind()) {
            case Global::Kind::FUNC:
            case Global::Kind::BLOCK: {
              return lhs_;
            }
            case Global::Kind::EXTERN: {
              return SymbolicValue::Pointer(l.Ptr.Decay());
            }
            case Global::Kind::ATOM: {
              auto *atom = static_cast<Atom *>(g->Symbol);
              if (auto align = atom->GetAlignment()) {
                if (rhs.getBitWidth() <= 64) {
                  if (align->value() % rhs.getSExtValue() + 1 == 0) {
                    llvm_unreachable("not implemented");
                  }
                }
              }
              return SymbolicValue::Value(l.Ptr.Decay());
            }
          }
          llvm_unreachable("not implemented");
        }
      }
      return SymbolicValue::Value(l.Ptr.Decay());
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
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}
