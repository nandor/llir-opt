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
  PhiInst(Type type, AnnotSet &&annot = {});
  PhiInst(Type type, const AnnotSet &annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Adds an incoming value.
  void Add(Block *block, Ref<Inst> value);

  /// Returns the number of predecessors.
  unsigned GetNumIncoming() const;

  /// Updates the nth block.
  void SetBlock(unsigned i, Block *block);
  /// Returns the nth block.
  const Block *GetBlock(unsigned i) const;
  /// Returns the nth block.
  Block *GetBlock(unsigned i);

  /// Sets the value attached to a block.
  void SetValue(unsigned i, Ref<Inst> value);
  /// Returns the nth value.
  ConstRef<Inst> GetValue(unsigned i) const;
  /// Returns the nth value.
  Ref<Inst> GetValue(unsigned i);
  /// Returns an operand for a block.
  Ref<Inst> GetValue(const Block *block);
  /// Returns an operand for a block.
  inline ConstRef<Inst> GetValue(const Block *block) const
  {
    return const_cast<PhiInst *>(this)->GetValue(block);
  }

  /// Removes an incoming value.
  void Remove(const Block *block);
  /// Checks if the PHI has a value for a block.
  bool HasValue(const Block *block) const;

  /// Returns the immediate type.
  Type GetType() const { return type_; }

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }

private:
  /// Type of the PHI node.
  Type type_;
};
