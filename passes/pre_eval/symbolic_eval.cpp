// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/expr.h"

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"
#include "passes/pre_eval/symbolic_heap.h"



// -----------------------------------------------------------------------------
void SymbolicEval::VisitInst(Inst &i)
{
  i.dump();
  for (auto op : i.operand_values()) {
    if (auto inst = ::cast_or_null<Inst>(op)) {
      llvm::errs() << "\t" << ctx_.Lookup(inst) << "\n";
    }
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitBarrierInst(BarrierInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitMemoryLoadInst(MemoryLoadInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitMemoryStoreInst(MemoryStoreInst &i)
{
  auto valueRef = i.GetValue();
  auto valueType = valueRef.GetType();
  auto value = ctx_.Lookup(valueRef);
  auto addr = ctx_.Lookup(i.GetAddr());

  switch (addr.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::POINTER: {
      heap_.Store(addr.GetPointer(), value, valueType);
      return;
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitMemoryExchangeInst(MemoryExchangeInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitLoadLinkInst(LoadLinkInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitStoreCondInst(StoreCondInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitArgInst(ArgInst &i)
{
  llvm::errs() << "\tTODO\n";
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitMovInst(MovInst &i)
{
  auto arg = i.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      i.dump();
      llvm_unreachable("not implemented");
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
          switch (i.GetType()) {
            case Type::I8:
            case Type::I16:
            case Type::I32:
            case Type::I64:
            case Type::V64:
            case Type::I128: {
              auto &ci = static_cast<ConstantInt &>(c);
              ctx_.Map(i, SymbolicValue::Integer(ci.GetValue()));
              return;
            }
            case Type::F32:
            case Type::F64:
            case Type::F80:
            case Type::F128: {
              llvm_unreachable("not implemented");
              return;
            }
          }
          llvm_unreachable("invalid integer type");
        }
        case Constant::Kind::FLOAT: {
          llvm_unreachable("not implemented");
          return;
        }
      }
      llvm_unreachable("invalid constant kind");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitMovGlobal(Inst &i, Global &g, int64_t offset)
{
  switch (g.GetKind()) {
    case Global::Kind::FUNC: {
      llvm_unreachable("not implemented");
      return;
    }
    case Global::Kind::BLOCK: {
      llvm_unreachable("not implemented");
      return;
    }
    case Global::Kind::ATOM: {
      ctx_.Map(i, SymbolicValue::Address(&g, offset));
      return;
    }
    case Global::Kind::EXTERN: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitSllInst(SllInst &i)
{
  class Visitor final : public SymbolicBinaryVisitor {
  public:
    SymbolicValue Visit(
        const Inst &i,
        const APInt &lhs,
        const APInt &rhs) override
    {
      llvm_unreachable("not implemented");
    }
  };
  ctx_.Map(i, Visitor().Dispatch(ctx_, i));
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitAddInst(AddInst &i)
{
  class Visitor final : public SymbolicBinaryVisitor {
  public:
    SymbolicValue Visit(
        const Inst &i,
        const APInt &lhs,
        const APInt &rhs) override
    {
      llvm_unreachable("not implemented");
    }
  };
  ctx_.Map(i, Visitor().Dispatch(ctx_, i));
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitAndInst(AndInst &i)
{
  class Visitor final : public SymbolicBinaryVisitor {
  public:
    SymbolicValue Visit(
        const Inst &i,
        const APInt &lhs,
        const APInt &rhs) override
    {
      llvm_unreachable("not implemented");
    }
  };
  ctx_.Map(i, Visitor().Dispatch(ctx_, i));
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitX86_WrMsrInst(X86_WrMsrInst &i)
{
  llvm::errs() << "\tTODO\n";
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitX86_RdTscInst(X86_RdTscInst &i)
{
  ctx_.Map(i, SymbolicValue::Unknown());
}
