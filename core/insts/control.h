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

public:
  ReturnInst(llvm::ArrayRef<Ref<Inst>> values, AnnotSet &&annot);

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
  RaiseInst(
      std::optional<CallingConv> conv,
      Ref<Inst> target,
      Ref<Inst> stack,
      llvm::ArrayRef<Ref<Inst>> values,
      AnnotSet &&annot
  );

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
  Block *getSuccessor(unsigned i) override;
  /// Returns a successor.
  inline const Block *getSuccessor(unsigned idx) const
  {
    return const_cast<SwitchInst *>(this)->getSuccessor(idx);
  }

  /// Returns the index value.
  ConstRef<Inst> GetIdx() const;
  /// Returns the index value.
  Ref<Inst> GetIdx();

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }

  /// Return the number of arguments.
  size_t block_size() const { return getNumSuccessors(); }
  /// Return the nth block.
  const Block *block(unsigned i) const { return getSuccessor(i); }
  /// Start of the blockument list.
  block_iterator block_begin() { return block_iterator(this->value_op_begin() + 1); }
  /// End of the blockument list.
  block_iterator block_end() { return block_iterator(this->value_op_end()); }
  /// Range of blockuments.
  block_range blocks() { return llvm::make_range(block_begin(), block_end()); }
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
  Block *getSuccessor(unsigned i) override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};


/**
 * Landing pad instruction for exception handling.
 *
 * Introduces values transferred from the raise site through registers.
 */
class LandingPadInst final : public ControlInst {
public:
  using type_iterator = std::vector<Type>::iterator;
  using const_type_iterator = std::vector<Type>::const_iterator;

  using type_range = llvm::iterator_range<type_iterator>;
  using const_type_range = llvm::iterator_range<const_type_iterator>;

public:
  /// Constructs a landing pad instruction.
  LandingPadInst(
      llvm::ArrayRef<Type> types,
      std::optional<CallingConv> conv,
      AnnotSet &&annot
  );
  /// Constructs a landing pad instruction.
  LandingPadInst(
      llvm::ArrayRef<Type> types,
      std::optional<CallingConv> conv,
      const AnnotSet &annot
  );

  /// Returns the calling convention.
  std::optional<CallingConv> GetCallingConv() const { return conv_; }

  /// Terminators do not return values.
  unsigned GetNumRets() const override { return types_.size(); }
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Iterators over types.
  size_t type_size() const { return types_.size(); }
  /// Check whether the function returns any values.
  bool type_empty() const { return types_.empty(); }
  /// Accessor to a given type.
  Type type(unsigned i) const { return types_[i]; }
  /// Start of type list.
  type_iterator type_begin() { return types_.begin(); }
  const_type_iterator type_begin() const { return types_.begin(); }
  /// End of type list.
  type_iterator type_end() { return types_.end(); }
  const_type_iterator type_end() const { return types_.end(); }
  /// Range of types.
  type_range types() { return llvm::make_range(type_begin(), type_end()); }
  const_type_range types() const { return llvm::make_range(type_begin(), type_end()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }

private:
  /// Calling convention.
  std::optional<CallingConv> conv_;
  /// Values returned by the landing pad.
  std::vector<Type> types_;
};
