// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <string_view>

#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>

#include "core/value.h"

class Prog;
class Block;



/**
 * GenericMachine function.
 */
class Func final : public llvm::ilist_node<Func>, public Value {
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
  Func(Prog *prog, const std::string &name);

  /**
   * Adds a new anonymous basic block.
   */
  void AddBlock(Block *block);

  /**
   * Sets the size of the function's stack.
   */
  void SetStackSize(size_t stackSize);

  /**
   * Returns the size of the stack.
   */
  size_t GetStackSize() const { return stackSize_; }

  /**
   * Returns the name of the function.
   */
  std::string_view GetName() const { return name_; }

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
  /// Name of the function.
  std::string name_;
  /// Size of the stack.
  size_t stackSize_;
};
