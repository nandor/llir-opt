// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/IR/CFG.h>
#include <llvm/Support/raw_ostream.h>

#include "core/inst.h"

class Func;



/**
 * Basic block.
 */
class Block : public llvm::ilist_node_with_parent<Block, Func> {
public:
  // Type of the instruction list.
  using InstListType = llvm::ilist<Inst>;

  // Iterator types over instructions.
  using iterator = InstListType::iterator;
  using reverse_iterator = InstListType::reverse_iterator;
  using const_iterator = InstListType::const_iterator;
  using const_reverse_iterator = InstListType::const_reverse_iterator;

  // Iterator over connected basic blocks.
  using succ_iterator = llvm::SuccIterator<TerminatorInst, Block>;
  using pred_iterator = std::vector<Block *>::iterator;

public:
  /**
   * Creates a new basic block.
   *
   * @param parent Parent function.
   * @param name   Name of the basic block.
   */
  Block(Func *parent, const std::string_view name);

  /// Adds an instruction to the basic block.
  void AddInst(Inst *inst);

  /// Returns the name of the basic block.
  std::string_view GetName() const { return name_; }

  /// Returns a pointer to the parent block.
  Func *getParent() const { return parent_; }

  /// Checks if the block is empty.
  bool IsEmpty() const { return insts_.empty(); }

  /// Returns the terminator of the block.
  TerminatorInst *GetTerminator();

  // Iterator over the instructions.
  iterator begin() { return insts_.begin(); }
  iterator end() { return insts_.end(); }
  const_iterator begin() const { return insts_.begin(); }
  const_iterator end() const { return insts_.end(); }
  reverse_iterator rbegin() { return insts_.rbegin(); }
  reverse_iterator rend() { return insts_.rend(); }
  const_reverse_iterator rbegin() const { return insts_.rbegin(); }
  const_reverse_iterator rend() const { return insts_.rend(); }

  // Iterator over the successors.
  succ_iterator succ_begin();
  succ_iterator succ_end();
  inline llvm::iterator_range<succ_iterator> successors() {
    return llvm::make_range(succ_begin(), succ_end());
  }
  inline unsigned succ_size() {
    return std::distance(succ_begin(), succ_end());
  }

  // Iterator over the predecessors.
  pred_iterator pred_begin();
  pred_iterator pred_end();

  // LLVM debug printing.
  void printAsOperand(llvm::raw_ostream &O, bool PrintType = true) const;

private:
  /// Parent function.
  Func *parent_;
  /// List of instructions.
  InstListType insts_;
  /// Name of the block.
  std::string name_;
};
