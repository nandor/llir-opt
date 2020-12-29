// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"



/**
 * Token for unknown values.
 */
struct Unknown {};


/**
 * Visitor for binary values.
 */
class SymbolicBinaryVisitor {
public:
  /// Looks up the values and dispatches them to the correct case.
  SymbolicValue Dispatch(SymbolicContext &ctx, const BinaryInst &i);

protected:
  virtual SymbolicValue Visit(
      const Inst &i,
      Unknown lhs,
      Unknown rhs)
  {
    return SymbolicValue::Unknown();
  }

  virtual SymbolicValue Visit(
      const Inst &i,
      Unknown lhs,
      const APInt &rhs)
  {
    return SymbolicValue::Unknown();
  }

  virtual SymbolicValue Visit(
      const Inst &i,
      Unknown lhs,
      const SymbolicPointer &rhs)
  {
    return SymbolicValue::Unknown();
  }

  virtual SymbolicValue Visit(
      const Inst &i,
      const APInt &lhs,
      Unknown rhs)
  {
    return SymbolicValue::Unknown();
  }

  virtual SymbolicValue Visit(
      const Inst &i,
      const APInt &lhs,
      const APInt &rhs)
  {
    return SymbolicValue::Unknown();
  }

  virtual SymbolicValue Visit(
      const Inst &i,
      const APInt &lhs,
      const SymbolicPointer &rhs)
  {
    return SymbolicValue::Unknown();
  }

  virtual SymbolicValue Visit(
      const Inst &i,
      const SymbolicPointer &lhs,
      Unknown rhs)
  {
    return SymbolicValue::Unknown();
  }

  virtual SymbolicValue Visit(
      const Inst &i,
      const SymbolicPointer &lhs,
      const APInt &rhs)
  {
    return SymbolicValue::Unknown();
  }

  virtual SymbolicValue Visit(
      const Inst &i,
      const SymbolicPointer &lhs,
      const SymbolicPointer &rhs)
  {
    return SymbolicValue::Unknown();
  }
};
