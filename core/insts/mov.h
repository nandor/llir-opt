// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <optional>

#include <llvm/ADT/iterator.h>

#include "core/inst.h"


/**
 * MovInst
 */
class MovInst final : public OperatorInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::MOV;

public:
  MovInst(Type type, Ref<Value> op, AnnotSet &&annot);
  MovInst(Type type, Ref<Value> op, const AnnotSet &annot);

  ~MovInst();

  /// Returns the value read/moved.
  ConstRef<Value> GetArg() const;
  /// Returns the value read/moved.
  Ref<Value> GetArg();

  /// Instruction is constant if argument is.
  bool IsConstant() const override { return !GetArg()->Is(Value::Kind::INST); }
};
