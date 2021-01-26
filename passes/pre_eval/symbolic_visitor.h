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
  BinaryVisitor(SymbolicEval &eval, T &i)
    : eval_(eval)
    , ctx_(eval_.GetContext())
    , inst_(i)
    , lhs_(eval_.Find(i.GetLHS()))
    , rhs_(eval_.Find(i.GetRHS()))
  {
  }

  /// Dispatch to the correct case.
  bool Evaluate();

protected:
  /// Token for an undefined value.
  struct Undefined {};
  /// Token for unknown integer values.
  struct Scalar {};
  /// Token for lower bounded integers.
  struct LowerBoundedInteger { const APInt &Bound; };
  /// Token for masked integers.
  struct Mask { const APInt &Known; const APInt &Value; };
  /// Token for values.
  struct Value { const SymbolicPointer::Ref &Ptr; };
  /// Token for pointers.
  struct Pointer { const SymbolicPointer::Ref &Ptr; };
  /// Token for pointer or null values.
  struct Nullable { const SymbolicPointer::Ref &Ptr; };

  #define VISITOR(lhs, rhs)                                                    \
    virtual bool Visit(lhs, rhs)                                               \
    { llvm_unreachable(("not implemented: " #lhs " and " #rhs)); }

  #define VISITOR_GROUP(lhs)                                                   \
    VISITOR(lhs, Scalar)                                                       \
    VISITOR(lhs, LowerBoundedInteger)                                          \
    VISITOR(lhs, Mask)                                                         \
    VISITOR(lhs, const APInt &)                                                \
    VISITOR(lhs, const APFloat &)                                              \
    VISITOR(lhs, Pointer)                                                      \
    VISITOR(lhs, Undefined)                                                    \
    VISITOR(lhs, Value)                                                        \
    VISITOR(lhs, Nullable)

  VISITOR_GROUP(Scalar);
  VISITOR_GROUP(LowerBoundedInteger);
  VISITOR_GROUP(Mask);
  VISITOR_GROUP(const APInt &);
  VISITOR_GROUP(const APFloat &);
  VISITOR_GROUP(Pointer);
  VISITOR_GROUP(Undefined);
  VISITOR_GROUP(Value);
  VISITOR_GROUP(Nullable);

  #undef VISITOR_GROUP
  #undef VISITOR

protected:
  /// Forward to evaluator, return an integer.
  bool SetInteger(const APInt &i) { return eval_.SetInteger(i); }
  /// Forward to evaluator, return a lower bounded integer.
  bool SetLowerBounded(const APInt &i) { return eval_.SetLowerBounded(i); }
  /// Forward to evaluator, return a undefined value.
  bool SetUndefined() { return eval_.SetUndefined(); }
  /// Forward to evaluator, return a scalar.
  bool SetScalar() { return eval_.SetScalar(); }
  /// Forward to evaluator, return a valu.
  bool SetPointer(const SymbolicPointer::Ref &p) { return eval_.SetPointer(p); }
  /// Forward to evaluator, return a nullable.
  bool SetNullable(const SymbolicPointer::Ref &p) { return eval_.SetNullable(p); }
  /// Forward to evaluator, return a pointer.
  bool SetValue(const SymbolicPointer::Ref &p) { return eval_.SetValue(p); }
  /// Forward to evaluator, return a pointer.
  bool SetMask(const APInt &k, const APInt &v) { return eval_.SetMask(k, v); }

protected:
  /// Reference to the evaluator.
  SymbolicEval &eval_;
  /// Reference to the context.
  SymbolicContext &ctx_;
  /// Instruction to be evaluated.
  T &inst_;
  /// Left-hand operand.
  const SymbolicValue &lhs_;
  /// Right-hand operand.
  const SymbolicValue &rhs_;
};

// -----------------------------------------------------------------------------
template <typename T>
bool BinaryVisitor<T>::Evaluate()
{
  #define DISPATCH(value)                                                      \
    switch (rhs_.GetKind()) {                                                  \
    case SymbolicValue::Kind::SCALAR:                                          \
      return Visit(value, Scalar{});                                           \
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:                           \
      return Visit(value, LowerBoundedInteger{rhs_.GetInteger()});             \
    case SymbolicValue::Kind::MASKED_INTEGER:                                  \
      return Visit(value, Mask{rhs_.GetMaskKnown(), rhs_.GetMaskValue()});     \
    case SymbolicValue::Kind::UNDEFINED:                                       \
      return Visit(value, Undefined{});                                        \
    case SymbolicValue::Kind::INTEGER:                                         \
      return Visit(value, rhs_.GetInteger());                                  \
    case SymbolicValue::Kind::FLOAT:                                           \
      return Visit(value, rhs_.GetFloat());                                    \
    case SymbolicValue::Kind::POINTER:                                         \
      return Visit(value, Pointer{rhs_.GetPointer()});                         \
    case SymbolicValue::Kind::NULLABLE:                                        \
      return Visit(value, Nullable{rhs_.GetPointer()});                        \
    case SymbolicValue::Kind::VALUE:                                           \
      return Visit(value, Value{rhs_.GetPointer()});                           \
    }                                                                          \
  llvm_unreachable("invalid rhs kind");

  switch (lhs_.GetKind()) {
    case SymbolicValue::Kind::SCALAR:
      DISPATCH((Scalar{}));
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
      DISPATCH((LowerBoundedInteger{lhs_.GetInteger()}));
    case SymbolicValue::Kind::MASKED_INTEGER:
      DISPATCH((Mask{lhs_.GetMaskKnown(), lhs_.GetMaskValue()}));
    case SymbolicValue::Kind::INTEGER:
      DISPATCH((lhs_.GetInteger()));
    case SymbolicValue::Kind::FLOAT:
      DISPATCH((lhs_.GetFloat()));
    case SymbolicValue::Kind::POINTER:
      DISPATCH((Pointer{lhs_.GetPointer()}));
    case SymbolicValue::Kind::VALUE:
      DISPATCH((Value{lhs_.GetPointer()}));
    case SymbolicValue::Kind::NULLABLE:
      DISPATCH((Nullable{lhs_.GetPointer()}));
    case SymbolicValue::Kind::UNDEFINED:
      DISPATCH((Undefined{}));
  }
  llvm_unreachable("invalid lhs kind");
#undef DISPATCH
}
