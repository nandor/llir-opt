// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/expr.h"
#include "core/atom.h"
#include "core/extern.h"

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"
#include "passes/pre_eval/symbolic_heap.h"



// -----------------------------------------------------------------------------
bool SymbolicEval::VisitArgInst(ArgInst &i)
{
  return ctx_.Set(i, ctx_.Arg(i.GetIndex()));
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitUndefInst(UndefInst &i)
{
  return SetUndefined();
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMovInst(MovInst &i)
{
  auto global = [&, this] (Global &g, int64_t off)
  {
    switch (g.GetKind()) {
      case Global::Kind::FUNC: {
        return SetPointer(std::make_shared<SymbolicPointer>(
            heap_.Function(static_cast<Func *>(&g))
        ));
      }
      case Global::Kind::BLOCK: {
        return SetPointer(std::make_shared<SymbolicPointer>(
            static_cast<Block *>(&g)
        ));
      }
      case Global::Kind::EXTERN: {
        return SetPointer(std::make_shared<SymbolicPointer>(
            static_cast<Extern *>(&g), off
        ));
      }
      case Global::Kind::ATOM: {
        auto &a = static_cast<Atom &>(g);
        return SetPointer(ctx_.Pointer(a, off));
      }
    }
    llvm_unreachable("invalid global kind");
  };

  auto arg = i.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      auto &val = ctx_.Find(::cast<Inst>(arg));
      if (::cast<Inst>(arg).GetType() == i.GetType()) {
        return ctx_.Set(i, val);
      } else {
        return ctx_.Set(i, val.Pin(i.GetSubValue(0), GetFrame()));
      }
    }
    case Value::Kind::GLOBAL: {
      if (IsPointerType(i.GetType())) {
        return global(*::cast<Global>(arg), 0ll);
      }
      llvm_unreachable("invalid global type");
    }
    case Value::Kind::EXPR: {
      auto &sym = *::cast<SymbolOffsetExpr>(arg);
      if (IsPointerType(i.GetType())) {
        return global(*sym.GetSymbol(), sym.GetOffset());
      }
      llvm_unreachable("invalid expression type");
    }
    case Value::Kind::CONST: {
      auto &c = *::cast<Constant>(arg);
      switch (c.GetKind()) {
        case Constant::Kind::INT: {
          auto val = static_cast<ConstantInt &>(c).GetValue();
          switch (auto ty = i.GetType()) {
            case Type::I8:
            case Type::I16:
            case Type::I32:
            case Type::I64:
            case Type::V64:
            case Type::I128: {
              auto w = GetBitWidth(ty);
              if (w != val.getBitWidth()) {
                return SetInteger(val.sextOrTrunc(w));
              } else {
                return SetInteger(val);
              }
            }
            case Type::F64: {
              return SetFloat(APFloat(APFloat::IEEEdouble(), val));
            }
            case Type::F32:
            case Type::F80:
            case Type::F128: {
              llvm_unreachable("not implemented");
            }
          }
          llvm_unreachable("invalid integer type");
        }
        case Constant::Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid constant kind");
    }
  }
  llvm_unreachable("invalid value kind");
}
