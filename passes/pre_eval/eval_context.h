// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>
#include <set>

#include "passes/pre_eval/symbolic_context.h"

class Block;




/**
 * A node in the DAG of the evaluation.
 */
struct BlockEvalNode {
  /// Flag indicating whether this is a loop to be over-approximated.
  bool IsLoop;
  /// Blocks which are part of the collapsed node.
  std::set<Block *> Blocks;
  /// Set of successor nodes.
  std::vector<BlockEvalNode *> Succs;
  /// Set of predecessor nodes.
  std::set<BlockEvalNode *> Preds;
  /// Length of the longest path to an exit.
  int64_t Length;
  /// Flag to indicate whether the node is on a path to return.
  bool Returns;
  /// Snapshot of the heap at this point.
  std::unique_ptr<SymbolicContext> Context;

  bool IsReturn() const;
};

/**
 * Print the eval node to a stream.
 */
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, BlockEvalNode &node);

/**
 * Evaluation context for a function.
 */
class EvalContext {
public:
  /**
   * Initialise the context required to evaluate a function.
   */
  EvalContext(Func &func);

  /**
   * Find the set of nodes and their originating contexts which reach
   * a join point after diverging on a bypassed path.
   */
  bool FindBypassed(
      std::set<BlockEvalNode *> &nodes,
      std::set<SymbolicContext *> &ctx,
      BlockEvalNode *start,
      BlockEvalNode *end
  );

  /// Return the function the context was built for.
  const Func &GetFunc() const { return func_; }
  /// Return the function the context was built for.
  Func &GetFunc() { return func_; }

public:
  /// Representation of all strongly-connected components.
  std::vector<std::unique_ptr<BlockEvalNode>> Nodes;
  /// Mapping from blocks to SCC nodes.
  std::unordered_map<Block *, BlockEvalNode *> BlockToNode;
  /// Block being executed.
  BlockEvalNode *Current = nullptr;
  /// Previous block.
  BlockEvalNode *Previous = nullptr;
  /// Set of executed nodes.
  std::set<BlockEvalNode *> ExecutedNodes;

private:
  /// Reference to the function.
  Func &func_;
};
