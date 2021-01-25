// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"



// -----------------------------------------------------------------------------
bool SymbolicEval::VisitBarrierInst(BarrierInst &i)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryLoadInst(MemoryLoadInst &i)
{
  auto addr = ctx_.Find(i.GetAddr());
  switch (addr.GetKind()) {
    case SymbolicValue::Kind::SCALAR:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
    case SymbolicValue::Kind::MASKED_INTEGER:
    case SymbolicValue::Kind::INTEGER:
    case SymbolicValue::Kind::UNDEFINED: {
      return SetUndefined();
    }
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::NULLABLE: {
      return ctx_.Set(i, ctx_.Load(*addr.GetPointer(), i.GetType()));
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

  ctx_.Taint(value, addr);

  switch (addr.GetKind()) {
    case SymbolicValue::Kind::SCALAR:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
    case SymbolicValue::Kind::INTEGER:
    case SymbolicValue::Kind::MASKED_INTEGER: {
      return false;
    }
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::NULLABLE: {
      return ctx_.Store(*addr.GetPointer(), value, valueType);
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
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitFrameInst(FrameInst &i)
{
  return SetValue(ctx_.Pointer(
      ctx_.GetActiveFrame()->GetIndex(),
      i.GetObject(),
      i.GetOffset()
  ));
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitVaStartInst(VaStartInst &va)
{
  llvm_unreachable("not implemented");
}
