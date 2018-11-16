// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string_view>

#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>

#include "core/value.h"
#include "core/global.h"
#include "core/calling_conv.h"

class Prog;
class Block;



/**
 * GenericMachine function.
 */
class Func final : public llvm::ilist_node<Func>, public Global {
public:
  /// Type of the block list.
  using BlockListType = llvm::ilist<Block>;

  /// Iterator over the blocks.
  using iterator = BlockListType::iterator;
  using const_iterator = BlockListType::const_iterator;

  // Pointer to the blocks field.
  static BlockListType Func::*getSublistAccess(Block*) {
    return &Func::blocks_;
  }

public:
  /**
   * Creates a new function.
   */
  Func(Prog *prog, const std::string_view name);

  /**
   * Adds a new anonymous basic block.
   */
  void AddBlock(Block *block);

  /// Sets the size of the function's stack.
  void SetStackSize(size_t stackSize);
  /// Returns the size of the stack.
  size_t GetStackSize() const { return stackSize_; }

  /// Sets the calling convention.
  void SetCallingConv(CallingConv conv) { callConv_ = conv; }
  /// Returns the calling convention.
  CallingConv GetCallingConv() const { return callConv_; }

  /// Sets the number of fixed args.
  void SetNumFixedArgs(unsigned args) { numFixedArgs_ = args; }
  /// Returns the number of fixed args.
  unsigned GetNumFixedArgs() const { return numFixedArgs_; }

  /**
   * Checks if the function has any blocks.
   */
  bool IsEmpty() const { return blocks_.empty(); }

  /**
   * Returns the size of the function.
   */
  size_t size() const { return blocks_.size(); }

  /// Returns the entry block.
  Block &getEntryBlock() { return *begin(); }
  const Block &getEntryBlock() const { return *begin(); }

  /// Checks if the function is a vararg function.
  bool IsVarArg() const { return false; }

  /// Functions are definitions.
  bool IsDefinition() const override { return true; }

  // Iterator over the blocks.
  iterator begin() { return blocks_.begin(); }
  iterator end() { return blocks_.end(); }
  const_iterator begin() const { return blocks_.begin(); }
  const_iterator end() const { return blocks_.end(); }

private:
  /// Name of the underlying program.
  Prog *prog_;
  /// Chain of basic blocks.
  BlockListType blocks_;
  /// Size of the stack.
  size_t stackSize_;
  /// Calling convention used by the function.
  CallingConv callConv_;
  /// Number of fixed arguments.
  unsigned numFixedArgs_;
};
