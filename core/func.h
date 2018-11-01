// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <string_view>
#include "adt/chain.h"

class Prog;
class Block;



/**
 * GenericMachine function.
 */
class Func final : public ChainNode<Func> {
private:
  using iterator = Chain<Block>::iterator;
  using const_iterator = Chain<Block>::const_iterator;

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
   * Returns the empty block.
   */
  const Block *getEmpty() const { return IsEmpty() ? nullptr : &*begin(); }

  // Iterator over the blocks.
  iterator begin() { return blocks_.begin(); }
  iterator end() { return blocks_.end(); }
  const_iterator begin() const { return blocks_.begin(); }
  const_iterator end() const { return blocks_.end(); }

private:
  /// Name of the underlying program.
  Prog *prog_;
  /// Name of the function.
  std::string name_;
  /// Size of the stack.
  size_t stackSize_;
  /// Chain of basic blocks.
  Chain<Block> blocks_;
};
