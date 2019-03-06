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
      const std::vector<SetNode *> &sets,
      const std::vector<DerefNode *> &derefs
  );

  /// Finds SCCs in the whole graph.
  SCCSolver &Full();

  /// Finds SCCs in a single node.
  SCCSolver &Single(SetNode *node);

  /// Traverses the groups.
  void Solve(std::function<void(const Group &)> &&f);

private:
  /// DFS implementing Tarjan's algorithm.
  void VisitFull(GraphNode *node);

  /// DFS implementing Tarjan's algorithm.
  void VisitSingle(SetNode *node);

private:
  /// All set nodes.
  const std::vector<SetNode *> &sets_;
  /// All deref nodes.
  const std::vector<DerefNode *> &derefs_;
  /// Traversal ID.
  uint32_t epoch_;
  /// Current index.
  uint32_t index_;
  /// Node stack.
  std::stack<GraphNode *> stack_;
  /// Components - stored since callback may change graph.
  std::vector<std::vector<GraphNode *>> sccs_;
};
