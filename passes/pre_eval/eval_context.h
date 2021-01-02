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
  std::vector<Block *> Blocks;
  /// Set of successor nodes.
  std::vector<BlockEvalNode *> Succs;
  /// Set of predecessor nodes.
  std::set<BlockEvalNode *> Preds;
  /// Length of the longest path to an exit.
  size_t Length;
  /// Snapshot of the heap at this point.
  std::unique_ptr<SymbolicContext> Context;
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
      BlockEvalNode *node
  );

public:
  /// Index of each function in reverse post-order.
  std::unordered_map<Block *, unsigned> Index;
  /// Representation of all strongly-connected components.
  std::vector<std::unique_ptr<BlockEvalNode>> Nodes;
  /// Mapping from blocks to SCC nodes.
  std::unordered_map<Block *, BlockEvalNode *> BlockToNode;
  /// Block being executed.
  BlockEvalNode *Current = nullptr;
  /// Previous block.
  BlockEvalNode *Previous = nullptr;
  /// Set of executed nodes.
  std::set<BlockEvalNode *> Executed;
};
