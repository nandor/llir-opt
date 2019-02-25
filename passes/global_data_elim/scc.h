// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <functional>
#include <stack>
#include <vector>

class Node;



/**
 * Helper to find SCCs.
 */
class SCCSolver final {
public:
  using NodeIter = std::vector<std::unique_ptr<Node>>::iterator;
  using Group = std::vector<Node *>;

public:
  /// Initialises the SCC solver.
  SCCSolver();

  /// Traverses SCC groups.
  void Solve(NodeIter begin, NodeIter end, std::function<void(const Group &)> &&f);

private:
  /// Graph traversal.
  void Connect(Node *node);

private:
  /// Current index.
  uint32_t index_;
  /// Node stack.
  std::stack<Node *> stack_;
  /// Callback function.
  std::function<void(const Group &)> f_;
};
