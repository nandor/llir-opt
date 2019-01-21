// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/block.h"
#include "core/inst.h"



/**
 * JumpCondInst
 */
class JumpCondInst final : public TerminatorInst {
public:
  JumpCondInst(Value *cond, Block *bt, Block *bf);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the condition.
  Inst *GetCond() const;
  /// Returns the true target.
  Block *GetTrueTarget() const;
  /// Returns the false target.
  Block *GetFalseTarget() const;

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
};

/**
 * JumpIndirectInst
 */
class JumpIndirectInst final : public TerminatorInst {
public:
  JumpIndirectInst(Inst *target);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the target.
  Inst *GetTarget() const { return static_cast<Inst *>(Op<0>().get()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
};

/**
 * JumpInst
 */
class JumpInst final : public TerminatorInst {
public:
  JumpInst(Block *target);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the target.
  Block *GetTarget() const { return static_cast<Block *>(Op<0>().get()); }

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
};

/**
 * ReturnInst
 */
class ReturnInst final : public TerminatorInst {
public:
  ReturnInst();
  ReturnInst(Type t, Inst *op);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the return value.
  Inst *GetValue() const;

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
};

/**
 * SwitchInst
 */
class SwitchInst final : public TerminatorInst {
public:
  SwitchInst(Inst *index, const std::vector<Block *> &branches);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the index value.
  Inst *GetIdx() const { return static_cast<Inst *>(Op<0>().get()); }

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
};

/**
 * Trap instruction which terminates a block.
 */
class TrapInst final : public TerminatorInst {
public:
  TrapInst() : TerminatorInst(Kind::TRAP, 0) { }

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
};
