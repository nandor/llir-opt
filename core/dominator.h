// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/iterator.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/Analysis/DominanceFrontier.h>
#include <llvm/Analysis/DominanceFrontierImpl.h>
#include <llvm/Support/GenericDomTree.h>
#include <llvm/Support/GenericDomTreeConstruction.h>

#include "core/block.h"
#include "core/func.h"



/// Traits for blocks.
template <>
struct llvm::GraphTraits<Block *> {
  using NodeRef = Block *;
  using ChildIteratorType = Block::succ_iterator;

  static NodeRef getEntryNode(Block *BB) { return BB; }
  static ChildIteratorType child_begin(NodeRef N) { return N->succ_begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->succ_end(); }
};

/// Reverse traits for blocks.
template <>
struct llvm::GraphTraits<llvm::Inverse<Block*>> {
  using NodeRef = Block *;
  using ChildIteratorType = Block::pred_iterator;

  static NodeRef getEntryNode(llvm::Inverse<Block *> G) { return G.Graph; }
  static ChildIteratorType child_begin(NodeRef N) { return N->pred_begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->pred_end(); }
};

/// Traits for functions.
template <>
struct llvm::GraphTraits<Func *> : public llvm::GraphTraits<Block *> {
  using nodes_iterator = pointer_iterator<Func::iterator>;

  static NodeRef getEntryNode(Func *F) { return &F->getEntryBlock(); }
  static nodes_iterator nodes_begin(Func *F) {
    return nodes_iterator(F->begin());
  }

  static nodes_iterator nodes_end(Func *F) {
    return nodes_iterator(F->end());
  }

  static size_t size(Func *F) { return F->size(); }
};

template <>
struct llvm::GraphTraits<llvm::Inverse<Func*>> : public llvm::GraphTraits<llvm::Inverse<Block*>> {
  static NodeRef getEntryNode(llvm::Inverse<Func *> G) {
    return &G.Graph->getEntryBlock();
  }
};



/**
 * Dominator tree for blocks.
 */
class DominatorTree : public llvm::DominatorTreeBase<Block, false> {
public:
  DominatorTree(Func &f) { recalculate(f); }
};

/**
 * Dominance frontier for blocks.
 */
class DominanceFrontier : public llvm::ForwardDominanceFrontierBase<Block> {
public:
  using DomTreeT       = llvm::DomTreeBase<Block>;
  using DomTreeNodeT   = llvm::DomTreeNodeBase<Block>;
  using DomSetType     = llvm::DominanceFrontierBase<Block, false>::DomSetType;
  using iterator       = llvm::DominanceFrontierBase<Block, false>::iterator;
  using const_iterator = llvm::DominanceFrontierBase<Block, false>::const_iterator;
};


namespace llvm {
namespace DomTreeBuilder {

using BlockDomTree = llvm::DominatorTreeBase<Block, false>;
extern template void Calculate<BlockDomTree>(BlockDomTree &DT);

} // end namespace DomTreeBuilder
} // end namespace llvm
