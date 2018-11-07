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
class PhiInst;
template<typename It, typename T>
using forward_it = llvm::iterator_facade_base<It, std::forward_iterator_tag, T>;


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

  /// Iterator over PHI nodes.
  template<typename PhiT, typename IterT>
  class PhiIterator : public forward_it<PhiIterator<PhiT, IterT>, PhiT> {
  public:
    /// Convert from non-const to const.
    template <typename PhiU, typename IterU>
    PhiIterator(const PhiIterator<PhiU, IterU> &rhs)
      : phi_(rhs.phi_)
    {
    }

    /// Check for end of iteration.
    bool operator == (const PhiIterator &rhs) const { return phi_ == rhs.phi_; }

    /// Return PHI node.
    PhiT &operator * () const { return *phi_; }

    /// Pre-increment.
    PhiIterator &operator ++ ()
    {
      auto it = std::next(IterT(phi_));
      if (it->Is(Inst::Kind::PHI)) {
        phi_ = static_cast<PhiT *>(&*it);
      } else {
        phi_ = nullptr;
      }
      return *this;
    }

    /// Post-increment.
    PhiIterator &operator ++ (int)
    {
      PhiIterator it = *this;
      *this++;
      return it;
    }

  private:
    /// Block can create an iterator.
    PhiIterator(PhiT *phi_) : phi_(phi_) {}

  private:
    friend Block;
    PhiT *phi_;
  };

  using phi_iterator = PhiIterator<PhiInst, iterator>;
  using const_phi_iterator = PhiIterator<const PhiInst, const_iterator>;

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

  // Iterator over PHI nodes.
  llvm::iterator_range<const_phi_iterator> phis() const {
    return const_cast<Block *>(this)->phis();
  }
  llvm::iterator_range<phi_iterator> phis();

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
