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

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Adds an incoming value.
  void Add(Block *block, Inst *value);
  /// Returns the number of predecessors.
  unsigned GetNumIncoming() const;
  /// Returns the nth block.
  Block *GetBlock(unsigned i)
  {
    return const_cast<Block *>(
        const_cast<const PhiInst *>(this)->GetBlock(i)
    );
  }
  /// Returns the nth block.
  const Block *GetBlock(unsigned i) const;

  /// Returns the nth value.
  Inst *GetValue(unsigned i)
  {
    return const_cast<Inst *>(
        const_cast<const PhiInst *>(this)->GetValue(i)
    );
  }
  /// Returns the nth value.
  const Inst *GetValue(unsigned i) const;

  /// Checks if there is a value for a block.
  bool HasValue(const Block *block);
  /// Removes an incoming value.
  void Remove(const Block *block);

  /// Updates the nth block.
  void SetBlock(unsigned i, Block *block);
  /// Sets the value attached to a block.
  void SetValue(unsigned i, Inst *value);

  /// Returns the immediate type.
  Type GetType() const { return type_; }
  /// Checks if the PHI has a value for a block.
  bool HasValue(const Block *block) const;

  /// Returns an operand for a block.
  Inst *GetValue(const Block *block)
  {
    return const_cast<Inst *>(
        const_cast<const PhiInst *>(this)->GetValue(block)
    );
  }
  /// Returns an operand for a block.
  const Inst *GetValue(const Block *block) const;

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
