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
    : ctx_(ctx)
    , inst_(i)
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
  struct Scalar {};
  /// Token for lower bounded integers.
  struct LowerBoundedInteger { const APInt &Bound; };
  /// Token for values.
  struct Value { const SymbolicPointer &Ptr; };
  /// Token for pointers.
  struct Pointer { const SymbolicPointer &Ptr; };
  /// Token for pointer or null values.
  struct Nullable { const SymbolicPointer &Ptr; };

  #define VISITOR(lhs, rhs, value) \
    virtual SymbolicValue Visit(lhs, rhs) { value; }

  VISITOR(Scalar, Scalar, return lhs_);
  VISITOR(Scalar, LowerBoundedInteger, return lhs_);
  VISITOR(Scalar, const APInt &, return lhs_);
  VISITOR(Scalar, const APFloat &, return lhs_);
  VISITOR(Scalar, Pointer, llvm_unreachable("not implemented"));
  VISITOR(Scalar, Undefined, llvm_unreachable("not implemented"));
  VISITOR(Scalar, Value, llvm_unreachable("not implemented"));
  VISITOR(Scalar, Nullable, llvm_unreachable("not implemented"));

  VISITOR(LowerBoundedInteger, Scalar, return rhs_);
  VISITOR(LowerBoundedInteger, LowerBoundedInteger,llvm_unreachable("not implemented"));
  VISITOR(LowerBoundedInteger, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(LowerBoundedInteger, const APFloat &, llvm_unreachable("not implemented"));
  VISITOR(LowerBoundedInteger, Pointer, llvm_unreachable("not implemented"));
  VISITOR(LowerBoundedInteger, Undefined, return rhs_);
  VISITOR(LowerBoundedInteger, Value, llvm_unreachable("not implemented"));
  VISITOR(LowerBoundedInteger, Nullable, llvm_unreachable("not implemented"));

  VISITOR(Undefined, Scalar, return rhs_);
  VISITOR(Undefined, LowerBoundedInteger, return rhs_);
  VISITOR(Undefined, const APInt &, return rhs_);
  VISITOR(Undefined, const APFloat &, return rhs_);
  VISITOR(Undefined, Pointer, return rhs_);
  VISITOR(Undefined, Undefined, return rhs_);
  VISITOR(Undefined, Value, llvm_unreachable("not implemented"));
  VISITOR(Undefined, Nullable, llvm_unreachable("not implemented"));

  VISITOR(const APInt &, Scalar, return rhs_);
  VISITOR(const APInt &, LowerBoundedInteger, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, Undefined, return rhs_);
  VISITOR(const APInt &, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, const APFloat &, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, Pointer, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, Value, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, Nullable, llvm_unreachable("not implemented"));

  VISITOR(const APFloat &, Scalar, return rhs_);
  VISITOR(const APFloat &, LowerBoundedInteger, llvm_unreachable("not implemented"));
  VISITOR(const APFloat &, Undefined, return rhs_);
  VISITOR(const APFloat &, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(const APFloat &, const APFloat &, llvm_unreachable("not implemented"));
  VISITOR(const APFloat &, Pointer, llvm_unreachable("not implemented"));
  VISITOR(const APFloat &, Value, llvm_unreachable("not implemented"));
  VISITOR(const APFloat &, Nullable, llvm_unreachable("not implemented"));

  VISITOR(Pointer, Scalar, llvm_unreachable("not implemented"));
  VISITOR(Pointer, LowerBoundedInteger, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Undefined, return rhs_);
  VISITOR(Pointer, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(Pointer, const APFloat &, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Pointer, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Value, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Nullable, llvm_unreachable("not implemented"));

  VISITOR(Value, Scalar, llvm_unreachable("not implemented"));
  VISITOR(Value, LowerBoundedInteger, llvm_unreachable("not implemented"));
  VISITOR(Value, Undefined, return rhs_);
  VISITOR(Value, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(Value, const APFloat &, llvm_unreachable("not implemented"));
  VISITOR(Value, Pointer, llvm_unreachable("not implemented"));
  VISITOR(Value, Value, llvm_unreachable("not implemented"));
  VISITOR(Value, Nullable, llvm_unreachable("not implemented"));

  VISITOR(Nullable, Scalar, llvm_unreachable("not implemented"));
  VISITOR(Nullable, LowerBoundedInteger, llvm_unreachable("not implemented"));
  VISITOR(Nullable, Undefined, return rhs_);
  VISITOR(Nullable, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(Nullable, const APFloat &, llvm_unreachable("not implemented"));
  VISITOR(Nullable, Pointer, llvm_unreachable("not implemented"));
  VISITOR(Nullable, Value, llvm_unreachable("not implemented"));
  VISITOR(Nullable, Nullable, llvm_unreachable("not implemented"));

protected:
  /// Reference to the context.
  SymbolicContext &ctx_;
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
  case SymbolicValue::Kind::SCALAR:                                            \
    return Visit(value, Scalar{});                                             \
  case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:                             \
    return Visit(value, LowerBoundedInteger{rhs_.GetInteger()});               \
  case SymbolicValue::Kind::UNDEFINED:                                         \
    return Visit(value, Undefined{});                                          \
  case SymbolicValue::Kind::INTEGER:                                           \
    return Visit(value, rhs_.GetInteger());                                    \
  case SymbolicValue::Kind::FLOAT:                                             \
    return Visit(value, rhs_.GetFloat());                                      \
  case SymbolicValue::Kind::POINTER:                                           \
    return Visit(value, Pointer{rhs_.GetPointer()});                           \
  case SymbolicValue::Kind::NULLABLE:                                          \
    return Visit(value, Nullable{rhs_.GetPointer()});                           \
  case SymbolicValue::Kind::VALUE:                                             \
    return Visit(value, Value{rhs_.GetPointer()});                             \
  }                                                                            \
  llvm_unreachable("invalid rhs kind");

  switch (lhs_.GetKind()) {
    case SymbolicValue::Kind::SCALAR:
      DISPATCH(Scalar{});
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
      DISPATCH(LowerBoundedInteger{lhs_.GetInteger()});
    case SymbolicValue::Kind::INTEGER:
      DISPATCH(lhs_.GetInteger());
    case SymbolicValue::Kind::FLOAT:
      DISPATCH(lhs_.GetFloat());
    case SymbolicValue::Kind::POINTER:
      DISPATCH(Pointer{lhs_.GetPointer()});
    case SymbolicValue::Kind::VALUE:
      DISPATCH(Value{lhs_.GetPointer()});
    case SymbolicValue::Kind::NULLABLE:
      DISPATCH(Nullable{lhs_.GetPointer()});
    case SymbolicValue::Kind::UNDEFINED:
      DISPATCH(Undefined{});
  }
  llvm_unreachable("invalid lhs kind");
}
