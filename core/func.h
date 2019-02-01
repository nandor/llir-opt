// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string_view>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>

#include "core/attr.h"
#include "core/global.h"
#include "core/type.h"
#include "core/value.h"

class Prog;
class Block;



/**
 * Traits to handle parent links from blocks.
 */
template <> struct llvm::ilist_traits<Block> {
private:
  using instr_iterator = simple_ilist<Block>::iterator;

public:
  void addNodeToList(Block *block);

  void removeNodeFromList(Block *block);

  void transferNodesFromList(
      ilist_traits &from,
      instr_iterator first,
      instr_iterator last
  );

  void deleteNode(Block *block);
};


/**
 * GenericMachine function.
 */
class Func final : public llvm::ilist_node<Func>, public Global {
public:
  /// Kind of the global.
  static constexpr Global::Kind kGlobalKind = Global::Kind::FUNC;

public:
  /// Type of the block list.
  using BlockListType = llvm::ilist<Block>;

  /// Iterator over the blocks.
  using iterator = BlockListType::iterator;
  using reverse_iterator = BlockListType::reverse_iterator;
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
   * Destroys the function.
   */
  ~Func() override;

  /// Removes a function from the program.
  void eraseFromParent();

  /// Adds a new anonymous basic block.
  void AddBlock(Block *block);

  /// Returns the parent block.
  Prog *getParent() const { return prog_; }

  /// Sets the size of the function's stack.
  void SetStackSize(size_t stackSize);
  /// Returns the size of the stack.
  size_t GetStackSize() const { return stackSize_; }

  /// Sets the calling convention.
  void SetCallingConv(CallingConv conv) { callConv_ = conv; }
  /// Returns the calling convention.
  CallingConv GetCallingConv() const { return callConv_; }

  /// Sets the vararg flag.
  void SetVarArg(bool varArg = true) { varArg_ = varArg; }
  /// Returns the vararg flags.
  bool IsVarArg() const { return varArg_; }

  /// Sets the alignment of the function.
  void SetAlignment(unsigned align) { align_ = align; }
  /// Returns the alignment of a function.
  unsigned GetAlignment() const { return align_; }

  /// Sets the visibilty of the function.
  void SetVisibility(Visibility visibility) { visibility_ = visibility; }
  /// Returns the visibilty of a function.
  Visibility GetVisibility() const { return visibility_; }
  /// Checks if a function is hidden.
  bool IsHidden() const { return GetVisibility() == Visibility::HIDDEN; }

  /// Sets the number of fixed args.
  void SetParameters(const std::vector<Type> &params) { params_ = params; }
  /// Returns the number of fixed args.
  llvm::ArrayRef<Type> GetParameters() const { return params_; }

  /// Erases a block.
  void erase(iterator it);
  /// Adds a block.
  void insertAfter(iterator it, Block *block);

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

  // Iterator over the blocks.
  iterator begin() { return blocks_.begin(); }
  iterator end() { return blocks_.end(); }
  const_iterator begin() const { return blocks_.begin(); }
  const_iterator end() const { return blocks_.end(); }

  // Reverse iterator over the blocks.
  reverse_iterator rbegin() { return blocks_.rbegin(); }
  reverse_iterator rend() { return blocks_.rend(); }

private:
  /// Name of the underlying program.
  Prog *prog_;
  /// Chain of basic blocks.
  BlockListType blocks_;
  /// Size of the stack.
  size_t stackSize_;
  /// Calling convention used by the function.
  CallingConv callConv_;
  /// Types of parameters.
  std::vector<Type> params_;
  /// Vararg flag.
  bool varArg_;
  /// Function alignment.
  unsigned align_;
  /// Function visibility.
  Visibility visibility_;
};
