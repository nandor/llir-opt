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
  /// Token for an undefined value.
  struct Undefined {};
  /// Token for unknown integer values.
  struct UnknownInteger {};
  /// Token for lower bounded integers.
  struct LowerBoundedInteger { const APInt &Bound; };
  /// Token for values.
  struct Value { const SymbolicPointer &Ptr; };
  /// Token for pointers.
  struct Pointer { const SymbolicPointer &Ptr; };

  #define VISITOR(lhs, rhs, value) \
    virtual SymbolicValue Visit(lhs, rhs) { value; }

  VISITOR(UnknownInteger, UnknownInteger, return lhs_);
  VISITOR(UnknownInteger, LowerBoundedInteger, return lhs_);
  VISITOR(UnknownInteger, const APInt &, return lhs_);
  VISITOR(UnknownInteger, Pointer, return lhs_);
  VISITOR(UnknownInteger, Undefined, return lhs_);
  VISITOR(UnknownInteger, Value, llvm_unreachable("not implemented"));

  VISITOR(LowerBoundedInteger, UnknownInteger, return rhs_);
  VISITOR(LowerBoundedInteger, LowerBoundedInteger,llvm_unreachable("not implemented"));
  VISITOR(LowerBoundedInteger, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(LowerBoundedInteger, Pointer, llvm_unreachable("not implemented"));
  VISITOR(LowerBoundedInteger, Undefined, return rhs_);
  VISITOR(LowerBoundedInteger, Value, llvm_unreachable("not implemented"));

  VISITOR(Undefined, UnknownInteger, return lhs_);
  VISITOR(Undefined, LowerBoundedInteger, return lhs_);
  VISITOR(Undefined, const APInt &, return lhs_);
  VISITOR(Undefined, Pointer, return lhs_);
  VISITOR(Undefined, Undefined, return lhs_);
  VISITOR(Undefined, Value, llvm_unreachable("not implemented"));

  VISITOR(const APInt &, UnknownInteger, return rhs_);
  VISITOR(const APInt &, LowerBoundedInteger, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, Undefined, return rhs_);
  VISITOR(const APInt &, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, Pointer, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, Value, llvm_unreachable("not implemented"));

  VISITOR(Pointer, UnknownInteger, llvm_unreachable("not implemented"));
  VISITOR(Pointer, LowerBoundedInteger, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Undefined, return rhs_);
  VISITOR(Pointer, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Pointer, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Value, llvm_unreachable("not implemented"));

  VISITOR(Value, UnknownInteger, llvm_unreachable("not implemented"));
  VISITOR(Value, LowerBoundedInteger, llvm_unreachable("not implemented"));
  VISITOR(Value, Undefined, return rhs_);
  VISITOR(Value, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(Value, Pointer, llvm_unreachable("not implemented"));
  VISITOR(Value, Value, llvm_unreachable("not implemented"));

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
  #define DISPATCH(value)                                                        \
    switch (rhs_.GetKind()) {                                                    \
    case SymbolicValue::Kind::UNKNOWN_INTEGER:                                   \
      return Visit(value, UnknownInteger{});                                     \
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:                             \
      return Visit(value, LowerBoundedInteger{rhs_.GetInteger()});                 \
    case SymbolicValue::Kind::UNDEFINED:                                         \
      return Visit(value, Undefined{});                                          \
    case SymbolicValue::Kind::INTEGER:                                           \
      return Visit(value, rhs_.GetInteger());                                    \
    case SymbolicValue::Kind::POINTER:                                           \
      return Visit(value, Pointer{rhs_.GetPointer()});                           \
    case SymbolicValue::Kind::VALUE:                                             \
      return Visit(value, Value{rhs_.GetPointer()});                             \
    }                                                                            \
    llvm_unreachable("invalid rhs kind");

  switch (lhs_.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN_INTEGER:
      DISPATCH(UnknownInteger{});
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
      DISPATCH(LowerBoundedInteger{lhs_.GetInteger()});
    case SymbolicValue::Kind::INTEGER:
      DISPATCH(lhs_.GetInteger());
    case SymbolicValue::Kind::POINTER:
      DISPATCH(Pointer{lhs_.GetPointer()});
    case SymbolicValue::Kind::VALUE:
      DISPATCH(Value{lhs_.GetPointer()});
    case SymbolicValue::Kind::UNDEFINED:
      DISPATCH(Undefined{});
  }
  llvm_unreachable("invalid lhs kind");
}
