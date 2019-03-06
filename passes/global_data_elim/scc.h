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
  using SetIter = std::vector<std::unique_ptr<SetNode>>::iterator;
  using Group = std::vector<GraphNode *>;

public:
  /// Initialises the SCC solver.
  SCCSolver(
      const std::vector<std::unique_ptr<SetNode>> &sets,
      const std::vector<std::unique_ptr<DerefNode>> &derefs
  );

  /// Finds SCCs in the whole graph.
  SCCSolver &Full();

  /// Finds SCCs in a single node.
  SCCSolver &Single(GraphNode *node);

  /// Traverses the groups.
  void Solve(std::function<void(const Group &)> &&f);

private:
  /// DFS implementing Tarjan's algorithm.
  void Traverse(GraphNode *node);

private:
  /// All set nodes.
  const std::vector<std::unique_ptr<SetNode>> &sets_;
  /// All deref nodes.
  const std::vector<std::unique_ptr<DerefNode>> &derefs_;
  /// Traversal ID.
  uint32_t epoch_;
  /// Current index.
  uint32_t index_;
  /// Node stack.
  std::stack<GraphNode *> stack_;
  /// Components - stored since callback may change graph.
  std::vector<std::vector<GraphNode *>> sccs_;
};
