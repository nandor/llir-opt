// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <llvm/ADT/iterator_range.h>

#include "core/adt/hash.h"
#include "core/analysis/union_find.h"

class Func;



/**
 * Class to compute the loop nesting forest.
 *
 * Implements a modified version of Havlak's algorithm, presented in:
 * "Identifying loops in almost linear time", G. Ramalingam, 1999
 */
class LoopNesting final {
public:
  /// Structure representing a loop.
  class Loop {
  public:
    /// Returns the loop header.
    const Block *GetHeader() const { return header_; }

    /// Iterator over loop children.
    using LoopIter = std::vector<Loop *>::iterator;
    LoopIter loop_begin() { return loops_.begin(); }
    LoopIter loop_end() { return loops_.end(); }
    llvm::iterator_range<LoopIter> loops()
    {
      return llvm::make_range(loop_begin(), loop_end());
    }

    /// Iterator over loop children.
    using BlockIter = std::vector<const Block *>::iterator;
    BlockIter block_begin() { return blocks_.begin(); }
    BlockIter block_end() { return blocks_.end(); }
    llvm::iterator_range<BlockIter> blocks()
    {
      return llvm::make_range(block_begin(), block_end());
    }

  private:
    friend class LoopNesting;
    /// Creates a new loop with a header.
    Loop(const Block *header) : header_(header), blocks_({header}) {}

  public:
    /// Header of the loop.
    const Block *header_;
    /// Loops in the loop.
    std::vector<Loop *> loops_;
    /// Nodes in the loop.
    std::vector<const Block *> blocks_;
  };

  /// Builds the loop nesting forest.
  LoopNesting(const Func *func);

  /// Checks if an edge is a loop edge.
  bool IsLoopEdge(const Block *a, const Block *b);

  /// Iterator over the loop nesting forest.
  std::set<Loop *>::iterator begin() { return roots_.begin(); }
  std::set<Loop *>::iterator end() { return roots_.end(); }

private:
  /// Finds a loop starting at a node.
  void FindLoop(unsigned header);

  /// Checks if an edge is a cross edge.
  bool IsCrossEdge(unsigned a, unsigned b);
  /// Checks if an edge is a tree edge.
  bool IsTreeEdge(unsigned a, unsigned b);
  /// Checks if an edge is a forward edge.
  bool IsForwardEdge(unsigned a, unsigned b);
  /// Checks if an edge is a back edge.
  bool IsBackEdge(unsigned a, unsigned b);

  /// Number all basic blocks.
  unsigned DFS(const Block *block, unsigned parent);
  /// Off-line LCA.
  void LCA(unsigned node);

  /// Marks irreducible loops.
  void MarkIrreducibleLoops(unsigned node);

private:
  /// Simple type for an edge.
  using Edge = std::pair<unsigned, unsigned>;

  /// Information about a node from DFS traversal.
  struct GraphNode {
    /// Parent node index.
    unsigned Parent;
    /// Actual block.
    const Block *BlockPtr;
    /// Loop the node is in.
    Loop *LoopPtr;
    /// Start index in the tree.
    unsigned Start;
    /// End index in traversal.
    unsigned End;

    /// Parent in the union-find structure.
    unsigned Link;
    /// Rank in the union-find structure.
    unsigned Rank;
    /// Representative of the class.
    unsigned Class;

    /// Incoming children.
    std::vector<unsigned> Pred;
    /// Children in the DFS tree.
    std::vector<unsigned> Children;
    /// Cross-forward candidates.
    std::vector<unsigned> CrossForwardCandidates;
    /// Cross-Forward edges.
    std::vector<Edge> CrossForwardEdges;
  };

  /// Number of nodes in the graph.
  unsigned n_;

  /// Reverse DFS order.
  std::vector<GraphNode> graph_;
  /// Index of the block in a preorder traversal.
  std::unordered_map<const Block *, unsigned> blockToId_;
  /// Counter for DFS indices.
  unsigned count_;

  /// Loop header unions.
  UnionFind loopHeaders_;
  /// Reducible loop header unions.
  UnionFind reducibleLoopHeaders_;

  /// LCA unions.
  UnionFind lcaParents_;
  /// LCA visited flag.
  std::vector<bool> lcaVisited_;
  /// LCA ancestors.
  std::vector<unsigned> lcaAncestor_;

  /// Flag to indicate if loops are reducible or not.
  std::vector<bool> irreducibleLoopHeader_;
  /// Loop parents of nodes.
  std::vector<std::optional<unsigned>> loopParent_;

  /// Loop nesting forest roots.
  std::set<Loop *> roots_;
  /// Block to loop mapping.
  std::unordered_map<const Block *, std::unique_ptr<Loop>> blockToLoop_;
};
