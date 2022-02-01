// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/calling_conv.h"
#include "core/block.h"
#include "core/inst.h"



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
