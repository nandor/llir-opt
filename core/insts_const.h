// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <optional>

#include <llvm/ADT/iterator.h>

#include "inst.h"



/**
 * ArgInst
 */
class ArgInst final : public ConstInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::ARG;

public:
  ArgInst(Type type, ConstantInt *index, const AnnotSet &annot);

  /// Returns the argument index.
  unsigned GetIdx() const;

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

/**
 * FrameInst
 */
class FrameInst final : public ConstInst {
public:
  FrameInst(
      Type type,
      ConstantInt *object,
      ConstantInt *index,
      const AnnotSet &annot
  );

  /// Returns the object identifier.
  unsigned GetObject() const;

  /// Returns the index.
  unsigned GetIndex() const;

  /// Instruction is constant.
  bool IsConstant() const override { return true; }
};

/**
 * Undefined value.
 */
class UndefInst final : public ConstInst {
public:
  UndefInst(Type type, const AnnotSet &annot);

  /// Instruction is constant.
  bool IsConstant() const override { return true; }
};

