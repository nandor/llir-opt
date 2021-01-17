// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>
#include <llvm/Support/Format.h>

#include "core/expr.h"
#include "core/atom.h"
#include "core/extern.h"

#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
bool SymbolicEval::Evaluate(Inst &inst)
{
  LLVM_DEBUG(llvm::dbgs() << inst << '\n');

  bool changed = Dispatch(inst);

  #ifndef NDEBUG
    for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
      if (auto value = ctx_.FindOpt(inst.GetSubValue(i))) {
        LLVM_DEBUG(llvm::dbgs() << "\t\t" << i << ": " << *value << '\n');
      }
    }
  #endif

  return changed;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitInst(Inst &i)
{
  llvm::errs() << "\n\nFAIL " << i << "\n";
  for (auto op : i.operand_values()) {
    if (auto inst = ::cast_or_null<Inst>(op)) {
      llvm::errs() << "\t" << ctx_.Find(inst) << "\n";
    }
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitLandingPadInst(LandingPadInst &i)
{
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitBarrierInst(BarrierInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryLoadInst(MemoryLoadInst &i)
{
  auto addr = ctx_.Find(i.GetAddr());
  switch (addr.GetKind()) {
    case SymbolicValue::Kind::SCALAR:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
    case SymbolicValue::Kind::INTEGER: {
      return ctx_.Set(i, SymbolicValue::Undefined());
    }
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::NULLABLE: {
      return ctx_.Set(i, ctx_.Load(addr.GetPointer(), i.GetType()));
    }
    case SymbolicValue::Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::FLOAT: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryStoreInst(MemoryStoreInst &i)
{
  auto valueRef = i.GetValue();
  auto valueType = valueRef.GetType();
  auto value = ctx_.Find(valueRef);
  auto addr = ctx_.Find(i.GetAddr());

  switch (addr.GetKind()) {
    case SymbolicValue::Kind::SCALAR:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
    case SymbolicValue::Kind::INTEGER: {
      return false;
    }
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::NULLABLE: {
      return ctx_.Store(addr.GetPointer(), value, valueType);
    }
    case SymbolicValue::Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::FLOAT: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryExchangeInst(MemoryExchangeInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitTerminatorInst(TerminatorInst &call)
{
  llvm_unreachable("cannot evaluate terminators");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitVaStartInst(VaStartInst &va)
{
  unsigned n = va.getParent()->getParent()->params().size();
  while (n < ctx_.GetNumArgs()) {
    auto v = ctx_.Arg(n++);
    llvm_unreachable("not implemented");
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitPhiInst(PhiInst &phi)
{
  llvm_unreachable("should be evaluated separately");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitArgInst(ArgInst &i)
{
  return ctx_.Set(i, ctx_.Arg(i.GetIndex()));
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMovInst(MovInst &i)
{
  auto arg = i.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      return ctx_.Set(i, ctx_.Find(::cast<Inst>(arg)));
    }
    case Value::Kind::GLOBAL: {
      if (IsPointerType(i.GetType())) {
        return VisitMovGlobal(i, *::cast<Global>(arg), 0ll);
      }
      llvm_unreachable("invalid global type");
    }
    case Value::Kind::EXPR: {
      auto &sym = *::cast<SymbolOffsetExpr>(arg);
      if (IsPointerType(i.GetType())) {
        return VisitMovGlobal(i, *sym.GetSymbol(), sym.GetOffset());
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
              auto width = GetSize(ty) * 8;
              if (width != val.getBitWidth()) {
                return ctx_.Set(i, SymbolicValue::Integer(val.trunc(width)));
              } else {
                return ctx_.Set(i, SymbolicValue::Integer(val));
              }
            }
            case Type::F64: {
              return ctx_.Set(i, SymbolicValue::Float(APFloat(
                  APFloat::IEEEdouble(),
                  val
              )));
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

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitBitCastInst(BitCastInst &i)
{
  auto v = ctx_.Find(i.GetArg());
  switch (i.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      llvm_unreachable("not implemented");
    }
    case Type::F64: {
      switch (v.GetKind()) {
        case SymbolicValue::Kind::SCALAR:
        case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
          llvm_unreachable("not implemented");
        }
        case SymbolicValue::Kind::INTEGER: {
          return ctx_.Set(i, SymbolicValue::Float(APFloat(
              APFloat::IEEEdouble(),
              v.GetInteger()
          )));
        }
        case SymbolicValue::Kind::VALUE:
        case SymbolicValue::Kind::POINTER:
        case SymbolicValue::Kind::NULLABLE: {
          llvm_unreachable("not implemented");
        }
        case SymbolicValue::Kind::UNDEFINED: {
          llvm_unreachable("not implemented");
        }
        case SymbolicValue::Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("not implemented");
    }
    case Type::F32:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitUndefInst(UndefInst &i)
{
  return ctx_.Set(i, SymbolicValue::Undefined());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMovGlobal(Inst &i, Global &g, int64_t off)
{
  switch (g.GetKind()) {
    case Global::Kind::FUNC: {
      return ctx_.Set(i, SymbolicValue::Pointer(static_cast<Func *>(&g)));
    }
    case Global::Kind::BLOCK: {
      return ctx_.Set(i, SymbolicValue::Pointer(static_cast<Block *>(&g)));
    }
    case Global::Kind::EXTERN: {
      return ctx_.Set(i, SymbolicValue::Pointer(static_cast<Extern *>(&g), off));
    }
    case Global::Kind::ATOM: {
      auto &a = static_cast<Atom &>(g);
      return ctx_.Set(i, SymbolicValue::Pointer(ctx_.Pointer(a, off)));
    }
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSelectInst(SelectInst &i)
{
  auto condVal = ctx_.Find(i.GetCond());
  auto trueVal = ctx_.Find(i.GetTrue());
  auto falseVal = ctx_.Find(i.GetFalse());
  if (condVal.IsTrue()) {
    return ctx_.Set(i, trueVal);
  }
  if (condVal.IsFalse()) {
    return ctx_.Set(i, falseVal);
  }
  return ctx_.Set(i, trueVal.LUB(falseVal));
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitFrameInst(FrameInst &i)
{
  return ctx_.Set(i, SymbolicValue::Value(ctx_.Pointer(
      ctx_.GetActiveFrame()->GetIndex(),
      i.GetObject(),
      i.GetOffset()
  )));
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitOUMulInst(OUMulInst &i)
{
  return ctx_.Set(i, SymbolicValue::Scalar());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitGetInst(GetInst &i)
{
  return ctx_.Set(i, SymbolicValue::Pointer(ctx_.GetActiveFrame()->GetIndex()));
}
