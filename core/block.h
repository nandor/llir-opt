// This file if part of the llir-opt project.
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
#include "core/global.h"
#include "core/symbol_table.h"

class Func;
class PhiInst;



/**
 * Basic block.
 */
class Block final : public llvm::ilist_node_with_parent<Block, Func>, public Global {
public:
  /// Kind of the global.
  static constexpr Global::Kind kGlobalKind = Global::Kind::BLOCK;

public:
  // Type of the instruction list.
  using InstListType = llvm::ilist<Inst>;

  // Iterator types over instructions.
  using iterator = InstListType::iterator;
  using reverse_iterator = InstListType::reverse_iterator;
  using const_iterator = InstListType::const_iterator;
  using const_reverse_iterator = InstListType::const_reverse_iterator;

  // Iterator wrapper.
  template<typename T>
  using iter_fwd = std::iterator<std::forward_iterator_tag, T, ptrdiff_t, T *, T *>;

  /// Iterator over the predecessors of a block.
  template <class BlockT, class UseIterator>
  class PredIterator : public iter_fwd<BlockT> {
  private:
    using Self = PredIterator<BlockT, UseIterator>;
    UseIterator use_;

  public:
    using pointer = typename iter_fwd<BlockT>::pointer;
    using reference = typename iter_fwd<BlockT>::reference;

    PredIterator() = default;

    inline PredIterator(UseIterator it) : use_(it) { SkipToTerminator(); }

    inline bool operator==(const PredIterator& x) const { return use_ == x.use_; }
    inline bool operator!=(const PredIterator& x) const { return !operator==(x); }

    inline reference operator*() const
    {
      return static_cast<const TerminatorInst *>(*use_)->getParent();
    }

    inline pointer *operator->() const
    {
      return &operator*();
    }

    inline PredIterator& operator++()
    {
      ++use_;
      SkipToTerminator();
      return *this;
    }

    inline PredIterator operator++(int) {
      PredIterator tmp = *this;
      ++*this;
      return tmp;
    }

  private:
    inline void SkipToTerminator()
    {
      while (!use_.atEnd()) {
        if (!*use_ || !(*use_)->Is(Value::Kind::INST)) {
          ++use_;
          continue;
        }

        if (!static_cast<const Inst *>(*use_)->IsTerminator()) {
          ++use_;
          continue;
        }

        break;
      }
    }
  };

  // Iterator over connected basic blocks.
  using succ_iterator = llvm::SuccIterator<TerminatorInst, Block>;
  using const_succ_iterator = llvm::SuccIterator<const TerminatorInst, const Block>;
  using pred_iterator = PredIterator<Block, Value::user_iterator>;
  using const_pred_iterator = PredIterator<const Block, Value::const_user_iterator>;

  // Forward iterator wrapper.
  template<typename It, typename T>
  using facade_fwd = llvm::iterator_facade_base<It, std::forward_iterator_tag, T>;

  /// Iterator over PHI nodes.
  template<typename PhiT, typename IterT>
  class PhiIterator : public facade_fwd<PhiIterator<PhiT, IterT>, PhiT> {
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
      auto end = phi_->getParent()->end();
      auto it = std::next(IterT(phi_));
      if (it != end && it->Is(Inst::Kind::PHI)) {
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
   * @param parent      Parent function.
   * @param visibility  Visibility attribute of the global symbol.
   */
  Block(
      const std::string_view name,
      Visibility visibility = Visibility::LOCAL
  );

  /**
   * Erases a basic block.
   */
  ~Block();

  /// Removes the global from the parent container.
  void removeFromParent() override;
  /// Removes a block from the parent.
  void eraseFromParent() override;

  /// Adds an instruction to the basic block.
  void AddInst(Inst *inst, Inst *before = nullptr);

  /// Adds a PHI instruction to the basic block.
  void AddPhi(PhiInst *phi);

  /// Returns a pointer to the parent block.
  Func *getParent() const { return parent_; }

  /// Returns the terminator of the block.
  TerminatorInst *GetTerminator();
  const TerminatorInst *GetTerminator() const;

  /// Blocks have no known alignment.
  std::optional<llvm::Align> GetAlignment() const override
  {
    return llvm::Align(1u);
  }

  /// Checks if the address of the block is taken.
  bool HasAddressTaken() const;
  /// Checks if the block is an exception landing pad.
  bool IsLandingPad() const;
  /// Checks if the block is a single trap.
  bool IsTrap() const;

  /// Add an instruction to the block.
  void insert(Inst *inst, iterator it);

  /// Removes an instruction.
  void remove(iterator it);
  /// Erases an instruction.
  void erase(iterator it);
  /// Erases a range of instructions.
  void erase(iterator first, iterator last);

  /// Clears all blocks.
  void clear();

  // Iterator over the instructions.
  bool empty() const { return insts_.empty(); }
  iterator begin() { return insts_.begin(); }
  iterator end() { return insts_.end(); }
  const_iterator begin() const { return insts_.begin(); }
  const_iterator end() const { return insts_.end(); }
  reverse_iterator rbegin() { return insts_.rbegin(); }
  reverse_iterator rend() { return insts_.rend(); }
  const_reverse_iterator rbegin() const { return insts_.rbegin(); }
  const_reverse_iterator rend() const { return insts_.rend(); }

  /// Returns the size of the block.
  size_t size() const { return insts_.size(); }

  // Iterator over the successors.
  succ_iterator succ_begin();
  succ_iterator succ_end();
  inline llvm::iterator_range<succ_iterator> successors()
  {
    return llvm::make_range(succ_begin(), succ_end());
  }

  const_succ_iterator succ_begin() const;
  const_succ_iterator succ_end() const;
  inline llvm::iterator_range<const_succ_iterator> successors() const
  {
    return llvm::make_range(succ_begin(), succ_end());
  }

  inline unsigned succ_size() const
  {
    return std::distance(succ_begin(), succ_end());
  }

  inline bool succ_empty() const { return succ_begin() == succ_end(); }

  // Iterator over the predecessors.
  inline bool pred_empty() const { return pred_begin() == pred_end(); }
  inline unsigned pred_size() const
  {
    return std::distance(pred_begin(), pred_end());
  }
  pred_iterator pred_begin() { return pred_iterator(user_begin()); }
  pred_iterator pred_end() { return pred_iterator(user_end()); }
  const_pred_iterator pred_begin() const { return const_pred_iterator(user_begin()); }
  const_pred_iterator pred_end() const { return const_pred_iterator(user_end()); }
  inline llvm::iterator_range<pred_iterator> predecessors()
  {
    return llvm::make_range(pred_begin(), pred_end());
  }
  inline llvm::iterator_range<const_pred_iterator> predecessors() const
  {
    return llvm::make_range(pred_begin(), pred_end());
  }

  // Checks whether the block lacks phi nodes.
  bool phi_empty() const { return !begin()->Is(Inst::Kind::PHI); }
  // Iterator over PHI nodes.
  llvm::iterator_range<const_phi_iterator> phis() const {
    return const_cast<Block *>(this)->phis();
  }
  llvm::iterator_range<phi_iterator> phis();

  /// Iterator to the first non-phi instruction.
  iterator first_non_phi();

  /// Split the block at the given iterator.
  Block *splitBlock(iterator I);

  // LLVM: Debug printing.
  void printAsOperand(llvm::raw_ostream &O, bool PrintType = true) const;

  /// Returns the program to which the extern belongs.
  Prog *getProg() override;

  /// Dumps the textual representation of the instruction.
  void dump(llvm::raw_ostream &os = llvm::errs()) const;

private:
  friend struct llvm::ilist_traits<Inst>;
  friend struct SymbolTableListTraits<Block>;
  /// Updates the parent node.
  void setParent(Func *parent) { parent_ = parent; }

private:
  /// Parent function.
  Func *parent_;
  /// List of instructions.
  InstListType insts_;
};

/// Print the value to a stream.
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const Block &block)
{
  block.dump(os);
  return os;
}
