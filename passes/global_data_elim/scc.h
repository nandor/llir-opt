// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <functional>
#include <stack>
#include <vector>

class GraphNode;



/**
 * Helper to find SCCs.
 */
class SCCSolver final {
public:
  using NodeIter = std::vector<std::unique_ptr<GraphNode>>::iterator;
  using Group = std::vector<GraphNode *>;

public:
  /// Initialises the SCC solver.
  SCCSolver();

  /// Traverses SCC groups.
  void Solve(NodeIter begin, NodeIter end, std::function<void(const Group &)> &&f);

private:
  /// Graph traversal.
  void Connect(GraphNode *node);

private:
  /// Current index.
  uint32_t index_;
  /// Node stack.
  std::stack<GraphNode *> stack_;
  /// Callback function.
  std::function<void(const Group &)> f_;
  /// Components - stored since callback may change graph.
  std::vector<std::vector<GraphNode *>> sccs_;
};
