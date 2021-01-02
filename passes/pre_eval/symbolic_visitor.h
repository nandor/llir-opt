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
  /// Token for values.
  struct Value { const SymbolicPointer &Ptr; };
  /// Token for pointers.
  struct Pointer { const SymbolicPointer &Ptr; };

  #define VISITOR(lhs, rhs, value) \
    virtual SymbolicValue Visit(lhs, rhs) { value; }

  VISITOR(UnknownInteger, UnknownInteger, return lhs_);
  VISITOR(UnknownInteger, const APInt &, return lhs_);
  VISITOR(UnknownInteger, Pointer, return lhs_);
  VISITOR(UnknownInteger, Undefined, return lhs_);
  VISITOR(UnknownInteger, Value, llvm_unreachable("not implemented"));

  VISITOR(Undefined, UnknownInteger, return lhs_);
  VISITOR(Undefined, const APInt &, return lhs_);
  VISITOR(Undefined, Pointer, return lhs_);
  VISITOR(Undefined, Undefined, return lhs_);
  VISITOR(Undefined, Value, llvm_unreachable("not implemented"));

  VISITOR(const APInt &, UnknownInteger, return rhs_);
  VISITOR(const APInt &, Undefined, return rhs_);
  VISITOR(const APInt &, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, Pointer, llvm_unreachable("not implemented"));
  VISITOR(const APInt &, Value, llvm_unreachable("not implemented"));

  VISITOR(Pointer, UnknownInteger, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Undefined, return rhs_);
  VISITOR(Pointer, const APInt &, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Pointer, llvm_unreachable("not implemented"));
  VISITOR(Pointer, Value, llvm_unreachable("not implemented"));

  VISITOR(Value, UnknownInteger, llvm_unreachable("not implemented"));
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
  switch (lhs_.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN_INTEGER: {
      switch (rhs_.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN_INTEGER: {
          return Visit(UnknownInteger{}, UnknownInteger{});
        }
        case SymbolicValue::Kind::UNDEFINED: {
          return Visit(UnknownInteger{}, Undefined{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(UnknownInteger{}, rhs_.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(UnknownInteger{}, Pointer{rhs_.GetPointer()});
        }
        case SymbolicValue::Kind::VALUE: {
          return Visit(UnknownInteger{}, Value{rhs_.GetPointer()});
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case SymbolicValue::Kind::INTEGER: {
      switch (rhs_.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN_INTEGER: {
          return Visit(lhs_.GetInteger(), UnknownInteger{});
        }
        case SymbolicValue::Kind::UNDEFINED: {
          return Visit(lhs_.GetInteger(), Undefined{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(lhs_.GetInteger(), rhs_.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(lhs_.GetInteger(), Pointer{rhs_.GetPointer()});
        }
        case SymbolicValue::Kind::VALUE: {
          return Visit(lhs_.GetInteger(), Value{rhs_.GetPointer()});
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case SymbolicValue::Kind::POINTER: {
      switch (rhs_.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN_INTEGER: {
          return Visit(Pointer{lhs_.GetPointer()}, UnknownInteger{});
        }
        case SymbolicValue::Kind::UNDEFINED: {
          return Visit(Pointer{lhs_.GetPointer()}, Undefined{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(Pointer{lhs_.GetPointer()}, rhs_.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(Pointer{lhs_.GetPointer()}, Pointer{rhs_.GetPointer()});
        }
        case SymbolicValue::Kind::VALUE: {
          return Visit(Pointer{lhs_.GetPointer()}, Value{rhs_.GetPointer()});
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case SymbolicValue::Kind::VALUE: {
      switch (rhs_.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN_INTEGER: {
          return Visit(Value{lhs_.GetPointer()}, UnknownInteger{});
        }
        case SymbolicValue::Kind::UNDEFINED: {
          return Visit(Value{lhs_.GetPointer()}, Undefined{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(Value{lhs_.GetPointer()}, rhs_.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(Value{lhs_.GetPointer()}, Pointer{rhs_.GetPointer()});
        }
        case SymbolicValue::Kind::VALUE: {
          return Visit(Value{lhs_.GetPointer()}, Value{rhs_.GetPointer()});
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case SymbolicValue::Kind::UNDEFINED: {
      switch (rhs_.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN_INTEGER: {
          return Visit(Undefined{}, UnknownInteger{});
        }
        case SymbolicValue::Kind::UNDEFINED: {
          return Visit(Undefined{}, Undefined{});
        }
        case SymbolicValue::Kind::INTEGER: {
          return Visit(Undefined{}, rhs_.GetInteger());
        }
        case SymbolicValue::Kind::POINTER: {
          return Visit(Undefined{}, Pointer{rhs_.GetPointer()});
        }
        case SymbolicValue::Kind::VALUE: {
          return Visit(Undefined{}, Value{rhs_.GetPointer()});
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
  }
  llvm_unreachable("invalid lhs kind");
}
