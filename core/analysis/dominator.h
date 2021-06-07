// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/iterator.h>
#include <llvm/Analysis/DominanceFrontier.h>
#include <llvm/Analysis/DominanceFrontierImpl.h>
#include <llvm/Support/GenericDomTree.h>
#include <llvm/Support/GenericDomTreeConstruction.h>

#include "core/cfg.h"
#include "core/block.h"
#include "core/func.h"



/**
 * Dominator tree for blocks.
 */
class DominatorTree : public llvm::DominatorTreeBase<Block, false> {
public:
  DominatorTree(Func &f) { recalculate(f); }

  bool Dominates(const Block *start, const Block *end, const Block *block)
  {
    if (!dominates(end, block)) {
      return false;
    }
    if (end->pred_size() == 1) {
      return true;
    }

    bool isDuplicateEdge = false;
    for (const Block *bb : end->predecessors()) {
      if (bb == start) {
        if (isDuplicateEdge) {
          return false;
        }
        isDuplicateEdge = true;
      } else {
        if (!dominates(end, bb)) {
          return false;
        }
      }
    }
    return true;
  }
};

/**
 * Dominator tree for blocks.
 */
class PostDominatorTree : public llvm::DominatorTreeBase<Block, true> {
public:
  PostDominatorTree(Func &f) { recalculate(f); }

  bool Dominates(const Block *start, const Block *end, const Block *block)
  {
    if (!dominates(start, block)) {
      return false;
    }
    if (start->succ_size() == 1) {
      return true;
    }

    bool isDuplicateEdge = false;
    for (const Block *bb : start->successors()) {
      if (bb == end) {
        if (isDuplicateEdge) {
          return false;
        }
        isDuplicateEdge = true;
      } else {
        if (!dominates(start, bb)) {
          return false;
        }
      }
    }
    return true;
  }
};


/**
 * Dominance frontier for blocks.
 */
class DominanceFrontier : public llvm::ForwardDominanceFrontierBase<Block> {
public:
  using iterator       = llvm::DominanceFrontierBase<Block, false>::iterator;
  using const_iterator = llvm::DominanceFrontierBase<Block, false>::const_iterator;
};

/**
 * Dominance frontier for blocks.
 */
class PostDominanceFrontier : public llvm::ReverseDominanceFrontierBase<Block> {
public:
  using iterator       = llvm::DominanceFrontierBase<Block, true>::iterator;
  using const_iterator = llvm::DominanceFrontierBase<Block, true>::const_iterator;
};



namespace llvm {
namespace DomTreeBuilder {

using BlockDomTree = llvm::DominatorTreeBase<Block, false>;
extern template void Calculate<BlockDomTree>(BlockDomTree &DT);

} // end namespace DomTreeBuilder
} // end namespace llvm
