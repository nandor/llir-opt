// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_visitor.h"


// -----------------------------------------------------------------------------
SymbolicValue SymbolicBinaryVisitor::Dispatch(
    SymbolicContext &ctx,
    const BinaryInst &i)
{
  const auto &lhs = ctx.Find(i.GetLHS());
  const auto &rhs = ctx.Find(i.GetRHS());
  switch (lhs.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN: {
      switch (rhs.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN: {
          return Visit(i, Unknown{}, Unknown{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(i, Unknown{}, rhs.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(i, Unknown{}, rhs.GetPointer());
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case SymbolicValue::Kind::INTEGER: {
      switch (rhs.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN: {
          return Visit(i, lhs.GetInteger(), Unknown{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(i, lhs.GetInteger(), rhs.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(i, lhs.GetInteger(), rhs.GetPointer());
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case SymbolicValue::Kind::POINTER: {
      switch (rhs.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN: {
          return Visit(i, lhs.GetPointer(), Unknown{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(i, lhs.GetPointer(), rhs.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(i, lhs.GetPointer(), rhs.GetPointer());
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
  }
  llvm_unreachable("invalid lhs kind");
}
