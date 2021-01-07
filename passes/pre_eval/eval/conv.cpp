// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"



// -----------------------------------------------------------------------------
bool SymbolicEval::VisitTruncInst(TruncInst &i)
{
  auto arg = ctx_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case SymbolicValue::Kind::UNDEFINED:
    case SymbolicValue::Kind::SCALAR: {
      return ctx_.Set(i, arg);
    }
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::INTEGER: {
      return ctx_.Set(i, SymbolicValue::Integer(
          arg.GetInteger().trunc(GetBitWidth(i.GetType()))
      ));
    }
    case SymbolicValue::Kind::FLOAT: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::NULLABLE: {
      return ctx_.Set(i, SymbolicValue::Scalar());
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitZExtInst(ZExtInst &i)
{
  auto arg = ctx_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case SymbolicValue::Kind::SCALAR:
    case SymbolicValue::Kind::UNDEFINED:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      return ctx_.Set(i, arg);
    }
    case SymbolicValue::Kind::INTEGER: {
      return ctx_.Set(i, SymbolicValue::Integer(
          arg.GetInteger().zext(GetBitWidth(i.GetType()))
      ));
    }
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::NULLABLE: {
      return ctx_.Set(i, arg);
    }
    case SymbolicValue::Kind::FLOAT: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSExtInst(SExtInst &i)
{
  auto arg = ctx_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case SymbolicValue::Kind::SCALAR:
    case SymbolicValue::Kind::UNDEFINED:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      return ctx_.Set(i, arg);
    }
    case SymbolicValue::Kind::INTEGER: {
      return ctx_.Set(i, SymbolicValue::Integer(
          arg.GetInteger().sext(GetBitWidth(i.GetType()))
      ));
    }
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::NULLABLE: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::FLOAT: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}
