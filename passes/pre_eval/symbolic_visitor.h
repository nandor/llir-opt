// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"


/**
 * Visitor for binary values.
 */
template<typename T>
class BinaryVisitor {
public:
  /// Looks up the values and dispatches them to the correct case.
  BinaryVisitor(SymbolicContext &ctx, const T &i)
    : inst_(i)
    , lhs_(ctx.Find(i.GetLHS()))
    , rhs_(ctx.Find(i.GetRHS()))
  {
  }

  /// Dispatch to the correct case.
  SymbolicValue Dispatch();

protected:
  /// Token for unknown values.
  struct Unknown {};
  /// Token for unknown integer values.
  struct UnknownInteger {};

  #define VISITOR(lhs, rhs) \
    virtual SymbolicValue Visit(lhs, rhs) { return SymbolicValue::Unknown(); }

  VISITOR(Unknown, Unknown)
  VISITOR(Unknown, UnknownInteger)
  VISITOR(Unknown, const APInt &)
  VISITOR(Unknown, const SymbolicPointer &)

  VISITOR(UnknownInteger, Unknown )
  VISITOR(UnknownInteger, UnknownInteger )
  VISITOR(UnknownInteger, const APInt &)
  VISITOR(UnknownInteger, const SymbolicPointer &)

  VISITOR(const APInt &, Unknown )
  VISITOR(const APInt &, UnknownInteger )
  VISITOR(const APInt &, const APInt &)
  VISITOR(const APInt &, const SymbolicPointer &)

  VISITOR(const SymbolicPointer &, Unknown )
  VISITOR(const SymbolicPointer &, UnknownInteger )
  VISITOR(const SymbolicPointer &, const APInt &)
  VISITOR(const SymbolicPointer &, const SymbolicPointer &)

protected:
  /// Instruction to be evaluated.
  const T &inst_;
  /// Left-hand operand.
  const SymbolicValue &lhs_;
  /// Right-hand operand.
  const SymbolicValue &rhs_;
};

// -----------------------------------------------------------------------------
template <typename T>
SymbolicValue BinaryVisitor<T>::Dispatch()
{
  switch (lhs_.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN: {
      switch (rhs_.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN: {
          return Visit(Unknown{}, Unknown{});
        }
        case SymbolicValue::Kind::UNKNOWN_INTEGER: {
          return Visit(Unknown{}, UnknownInteger{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(Unknown{}, rhs_.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(Unknown{}, rhs_.GetPointer());
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case SymbolicValue::Kind::UNKNOWN_INTEGER: {
      switch (rhs_.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN: {
          return Visit(UnknownInteger{}, Unknown{});
        }
        case SymbolicValue::Kind::UNKNOWN_INTEGER: {
          return Visit(UnknownInteger{}, UnknownInteger{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(UnknownInteger{}, rhs_.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(UnknownInteger{}, rhs_.GetPointer());
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case SymbolicValue::Kind::INTEGER: {
      switch (rhs_.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN: {
          return Visit(lhs_.GetInteger(), Unknown{});
        }
        case SymbolicValue::Kind::UNKNOWN_INTEGER: {
          return Visit(lhs_.GetInteger(), UnknownInteger{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(lhs_.GetInteger(), rhs_.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(lhs_.GetInteger(), rhs_.GetPointer());
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case SymbolicValue::Kind::POINTER: {
      switch (rhs_.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN: {
          return Visit(lhs_.GetPointer(), Unknown{});
        }
        case SymbolicValue::Kind::UNKNOWN_INTEGER: {
          return Visit(lhs_.GetPointer(), UnknownInteger{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(lhs_.GetPointer(), rhs_.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(lhs_.GetPointer(), rhs_.GetPointer());
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
  }
  llvm_unreachable("invalid lhs kind");
}
