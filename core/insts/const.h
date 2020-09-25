// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <optional>

#include <llvm/ADT/iterator.h>

#include "core/inst.h"



/**
 * Instruction referencing an argument.
 *
 * Retrieves the argument at a given index. The type of this instruction
 * must match the type encoded in the parent function.
 */
class ArgInst final : public ConstInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::ARG;

public:
  /// Constructs an argument instruction.
  ArgInst(Type type, ConstantInt *index, AnnotSet &&annot);
  /// Constructs an argument instruction.
  ArgInst(Type type, ConstantInt *index, const AnnotSet &annot);

  /// Returns the argument index.
  unsigned GetIdx() const;

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

/**
 * Instruction to derive a pointer into the frame.
 */
class FrameInst final : public ConstInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::FRAME;

public:
  FrameInst(
      Type type,
      ConstantInt *object,
      ConstantInt *index,
      AnnotSet &&annot
  );
  FrameInst(
      Type type,
      ConstantInt *object,
      ConstantInt *index,
      const AnnotSet &annot
  );

  /// Returns the object identifier.
  unsigned GetObject() const;

  /// Returns the index.
  unsigned GetOffset() const;

  /// Instruction is constant.
  bool IsConstant() const override { return true; }
};

/**
 * Undefined value.
 *
 * Undefined values are aggressively propagated and eliminated. Lowers
 * to ISD::UNDEF, allowing LLVM to further propagate it.
 */
class UndefInst final : public ConstInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::UNDEF;

public:
  UndefInst(Type type, AnnotSet &&annot);
  UndefInst(Type type, const AnnotSet &annot);

  /// Instruction is constant.
  bool IsConstant() const override { return true; }
};

/**
 * MovInst
 */
class MovInst final : public OperatorInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::MOV;

public:
  MovInst(Type type, Value *op, AnnotSet &&annot);
  MovInst(Type type, Value *op, const AnnotSet &annot);

  Value *GetArg() const { return static_cast<Value *>(Op<0>().get()); }

  /// Instruction is constant if argument is.
  bool IsConstant() const override { return !GetArg()->Is(Value::Kind::INST); }
};
