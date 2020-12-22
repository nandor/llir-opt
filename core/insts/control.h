// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/calling_conv.h"
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
  static constexpr Inst::Kind kInstKind = Inst::Kind::JUMP_COND;

public:
  JumpCondInst(Ref<Inst> cond, Block *bt, Block *bf, AnnotSet &&annot);
  JumpCondInst(Ref<Inst> cond, Block *bt, Block *bf, const AnnotSet &annot);

  /// Returns the successor node.
  const Block *getSuccessor(unsigned i) const override;
  /// Returns the successor node.
  Block *getSuccessor(unsigned i) override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the condition.
  ConstRef<Inst> GetCond() const;
  /// Returns the condition.
  Ref<Inst> GetCond();

  /// Returns the true target.
  const Block *GetTrueTarget() const;
  /// Returns the true target.
  Block *GetTrueTarget();

  /// Returns the false target.
  const Block *GetFalseTarget() const;
  /// Returns the false target.
  Block *GetFalseTarget();

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
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
  static constexpr Inst::Kind kInstKind = Inst::Kind::JUMP;

public:
  JumpInst(Block *target, AnnotSet &&annot);
  JumpInst(Block *target, const AnnotSet &annot);

  /// Returns the successor node.
  const Block *getSuccessor(unsigned i) const override;
  /// Returns the successor node.
  Block *getSuccessor(unsigned i) override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the target.
  const Block *GetTarget() const;
  /// Returns the target.
  Block *GetTarget();

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
  static constexpr Inst::Kind kInstKind = Inst::Kind::RETURN;

  using arg_iterator = conv_op_iterator<Inst>;
  using arg_range = conv_op_range<Inst>;
  using const_arg_iterator = const_conv_op_iterator<Inst>;
  using const_arg_range = const_conv_op_range<Inst>;

public:
  ReturnInst(llvm::ArrayRef<Ref<Inst>> values, AnnotSet &&annot);

  /// Returns the successor node.
  const Block *getSuccessor(unsigned i) const override;
  /// Returns the successor node.
  Block *getSuccessor(unsigned i) override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction does not return.
  bool IsReturn() const override { return true; }

  /// Returns an argument at an index.
  Ref<Inst> arg(unsigned i);
  /// Returns an argument at an index.
  ConstRef<Inst> arg(unsigned i) const
  {
    return const_cast<ReturnInst *>(this)->arg(i);
  }

  /// Returns the number of arguments.
  size_t arg_size() const;
  /// Checks if the return takes any arguments.
  bool arg_empty() const;
  /// Start of the argument list.
  arg_iterator arg_begin() { return arg_iterator(this->value_op_begin()); }
  /// End of the argument list.
  arg_iterator arg_end() { return arg_iterator(this->value_op_begin() + size()); }
  /// Range of arguments.
  arg_range args() { return llvm::make_range(arg_begin(), arg_end()); }
  /// Start of the argument list.
  const_arg_iterator arg_begin() const { return const_arg_iterator(this->value_op_begin()); }
  /// End of the argument list.
  const_arg_iterator arg_end() const { return const_arg_iterator(this->value_op_begin() + size()); }
  /// Range of arguments.
  const_arg_range args() const { return llvm::make_range(arg_begin(), arg_end()); }
};

/**
 * Long jump instruction.
 *
 * Used to implement longjmp: transfers control to the program point after the
 * setjmp call. The arguments include the target basic block, the stack pointer
 * to reset to and the value to return from the setjmp call.
 */
class RaiseInst final : public TerminatorInst {
public:
  using arg_iterator = conv_op_iterator<Inst>;
  using arg_range = conv_op_range<Inst>;
  using const_arg_iterator = const_conv_op_iterator<Inst>;
  using const_arg_range = const_conv_op_range<Inst>;

public:
  RaiseInst(
      std::optional<CallingConv> conv,
      Ref<Inst> target,
      Ref<Inst> stack,
      llvm::ArrayRef<Ref<Inst>> values,
      AnnotSet &&annot
  );

  /// Returns the successor node.
  const Block *getSuccessor(unsigned i) const override;
  /// Returns the successor node.
  Block *getSuccessor(unsigned i) override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the raise convention.
  std::optional<CallingConv> GetCallingConv() const { return conv_; }

  /// Returns the target.
  ConstRef<Inst> GetTarget() const;
  /// Returns the target.
  Ref<Inst> GetTarget();
  /// Returns the stack pointer.
  ConstRef<Inst> GetStack() const;
  /// Returns the stack pointer.
  Ref<Inst> GetStack();

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }

  /// Returns an argument at an index.
  Ref<Inst> arg(unsigned i);
  /// Returns an argument at an index.
  ConstRef<Inst> arg(unsigned i) const
  {
    return const_cast<RaiseInst *>(this)->arg(i);
  }
  /// Returns the number of arguments.
  size_t arg_size() const;
  /// Check if there are any arguments.
  bool arg_empty() const { return arg_size() == 0; }
  /// Start of the argument list.
  arg_iterator arg_begin() { return arg_iterator(this->value_op_begin() + 2); }
  /// End of the argument list.
  arg_iterator arg_end() { return arg_iterator(this->value_op_begin() + size()); }
  /// Range of arguments.
  arg_range args() { return llvm::make_range(arg_begin(), arg_end()); }
  /// Start of the argument list.
  const_arg_iterator arg_begin() const { return const_arg_iterator(this->value_op_begin() + 2); }
  /// End of the argument list.
  const_arg_iterator arg_end() const { return const_arg_iterator(this->value_op_begin() + size()); }
  /// Range of arguments.
  const_arg_range args() const { return llvm::make_range(arg_begin(), arg_end()); }

private:
  /// Calling convention to jump to.
  std::optional<CallingConv> conv_;
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

  using block_iterator = conv_op_iterator<Block>;
  using block_range = conv_op_range<Block>;
  using const_block_iterator = const_conv_op_iterator<Block>;
  using const_block_range = const_conv_op_range<Block>;

public:
  /// Constructs a switch instruction.
  SwitchInst(
      Ref<Inst> index,
      llvm::ArrayRef<Block *> branches,
      AnnotSet &&annot
  );
  /// Constructs a switch instruction.
  SwitchInst(
      Ref<Inst> index,
      llvm::ArrayRef<Block *> branches,
      const AnnotSet &annot
  );
  /// Constructs a switch instruction.
  SwitchInst(
      Ref<Inst> index,
      llvm::ArrayRef<Ref<Block>> branches,
      const AnnotSet &annot
  );

  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;
  /// Returns the successor node.
  const Block *getSuccessor(unsigned i) const override;
  /// Returns the successor node.
  Block *getSuccessor(unsigned i) override;

  /// Returns the index value.
  ConstRef<Inst> GetIndex() const;
  /// Returns the index value.
  Ref<Inst> GetIndex();

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }

  /// Return the number of arguments.
  size_t block_size() const { return getNumSuccessors(); }
  /// Return the nth block.
  const Block *block(unsigned i) const { return getSuccessor(i); }
  /// Start of the block list.
  block_iterator block_begin() { return block_iterator(this->value_op_begin() + 1); }
  const_block_iterator block_begin() const { return const_block_iterator(this->value_op_begin() + 1); }
  /// End of the block list.
  block_iterator block_end() { return block_iterator(this->value_op_end()); }
  const_block_iterator block_end() const { return const_block_iterator(this->value_op_end()); }
  /// Range of blocks.
  block_range blocks() { return llvm::make_range(block_begin(), block_end()); }
  const_block_range blocks() const { return llvm::make_range(block_begin(), block_end()); }
};
