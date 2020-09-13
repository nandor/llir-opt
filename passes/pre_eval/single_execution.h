// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/call_graph.h"

class Block;
class Func;




/**
 * An analysis to identify basic blocks that are executed only once.
 */
class SingleExecution final {
public:
  /// Initialises the analysis.
  SingleExecution(Func &f);

  /// Runs the analysis and returns the set of blocks.
  std::set<const Block *> Solve();

private:
  /// Mark the reachable nodes as part of a loop.
  void MarkInLoop(const CallGraph::Node *node);
  /// Mark all nodes reachable from a block as part of a loop.
  void MarkInLoop(const Block *block);
  /// Visit nodes which are outside of loops in a function.
  void Visit(const Func &f);

private:
  /// Entry method.
  Func &f_;
  /// Call graph.
  CallGraph g_;
  /// Blocks in loops.
  std::set<const Block *> inLoop_;
  /// Blocks not in loops.
  std::set<const Block *> singleExec_;
};
