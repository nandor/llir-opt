// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/block.h"
#include "core/inst.h"



/**
 * Conditional jump instruction.
 *
 * Accepts a flag. If the argument is zero, the false branch is taken,
 * otherwise the true branch is taken.
 */
class JumpCondInst final : public TerminatorInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::JCC;

public:
  JumpCondInst(Value *cond, Block *bt, Block *bf, AnnotSet &&annot);
  JumpCondInst(Value *cond, Block *bt, Block *bf, const AnnotSet &annot);

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
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * Indirect jump instruction.
 *
 * Accepts a single arguments, which should be a pointer to a basic block.
 * When the jump is executed, the basic block should be attached to a stack
 * frame which is live on the stack.
 */
class JumpIndirectInst final : public TerminatorInst {
public:
  JumpIndirectInst(Inst *target, AnnotSet &&annot);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the target.
  Inst *GetTarget() const { return static_cast<Inst *>(Op<0>().get()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * Unconditional jump instruction.
 *
 * Transfers control to a basic block in the same function.
 */
class JumpInst final : public TerminatorInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::JMP;

public:
  JumpInst(Block *target, AnnotSet &&annot);
  JumpInst(Block *target, const AnnotSet &annot);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the target.
  Block *GetTarget() const { return static_cast<Block *>(Op<0>().get()); }

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * ReturnInst
 */
class ReturnInst final : public TerminatorInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::RET;

public:
  ReturnInst(AnnotSet &&annot);
  ReturnInst(Inst *op, AnnotSet &&annot);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the return value.
  Inst *GetValue() const;

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction does not return.
  bool IsReturn() const override { return true; }
};

/**
 * Switch instruction
 *
 * Lowers to an efficient jump table. Takes a control index argument,
 * along with a table of successor blocks. If the control index is out of
 * bounds, behaviour is undefined.
 */
class SwitchInst final : public TerminatorInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::SWITCH;

public:
  /// Constructs a switch instruction.
  SwitchInst(
      Inst *index,
      const std::vector<Block *> &branches,
      AnnotSet &&annot
  );
  /// Constructs a switch instruction.
  SwitchInst(
      Inst *index,
      const std::vector<Block *> &branches,
      const AnnotSet &annot
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the index value.
  Inst *GetIdx() const { return static_cast<Inst *>(Op<0>().get()); }

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * Trap instruction which terminates a block.
 *
 * The trap instruction should never be reached by execution. It lowers to
 * an illegal instruction to aid debugging.
 */
class TrapInst final : public TerminatorInst {
public:
  /// Constructs a trap instruction.
  TrapInst(AnnotSet &&annot);
  /// Constructs a trap instruction.
  TrapInst(const AnnotSet &annot);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};
