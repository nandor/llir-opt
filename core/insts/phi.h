// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"



/**
 * PHI instruction.
 */
class PhiInst final : public Inst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::PHI;

public:
  PhiInst(Type type, const AnnotSet &annot = {});

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Adds an incoming value.
  void Add(Block *block, Value *value);
  /// Returns the number of predecessors.
  unsigned GetNumIncoming() const;
  /// Returns the nth block.
  Block *GetBlock(unsigned i) const;
  /// Returns the nth value.
  Value *GetValue(unsigned i) const;
  /// Checks if there is a value for a block.
  bool HasValue(const Block *block);
  /// Removes an incoming value.
  void Remove(const Block *block);

  /// Updates the nth block.
  void SetBlock(unsigned i, Block *block);
  /// Sets the value attached to a block.
  void SetValue(unsigned i, Value *value);

  /// Returns the immediate type.
  Type GetType() const { return type_; }
  /// Checks if the PHI has a value for a block.
  bool HasValue(const Block *block) const;
  /// Returns an operand for a block.
  Value *GetValue(const Block *block) const;

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }

private:
  /// Type of the PHI node.
  Type type_;
};
