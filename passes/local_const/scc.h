// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <stack>
#include <variant>
#include <vector>

#include "core/adt/id.h"

class LCSet;
class LCDeref;
class LCGraph;
class LCSCC;


/**
 * SCC metadata.
 */
class LCNode {
protected:
  /// Initialise the node.
  LCNode() : Epoch(0), Index(0), Link(0), InComponent(false) {}

private:
  friend class LCSCC;
  /// Epoch the node was visited in.
  uint32_t Epoch;
  /// Index on the stack.
  uint32_t Index;
  /// Lowest link.
  uint32_t Link;
  /// Flag to indicate if node on stack.
  bool InComponent;
};

/**
 * SCC over the LCGraph.
 */
class LCSCC {
public:
  using SetGroup = std::vector<ID<LCSet>>;
  using DerefGroup = std::vector<ID<LCDeref>>;

public:
  /// Initialises the SCC solver.
  LCSCC(LCGraph &graph);

  /// Finds SCCs in the whole graph.
  LCSCC &Full();

  /// Finds SCCs from a single node.
  LCSCC &Single(LCSet *node);

  /// Traverses the groups.
  void Solve(std::function<void(const SetGroup &, const DerefGroup &)> &&f);

private:
  /// DFS on the full graph, set nodes.
  void VisitFull(LCSet *node);
  /// DFS on the full graph, deref nodes.
  void VisitFull(LCDeref *node);
  /// Partial visitor, set nodes.
  void VisitSingle(LCSet *node);
  /// Visitor component.
  template <typename T> void Pre(T *node);
  /// Pops the stack as part of Tarjan's algorithm.
  template <typename T> void Post(T *node, ID<T> id);

private:
  /// Graph operated on.
  LCGraph &graph_;
  /// Current epoch.
  uint32_t epoch_;
  /// Current index.
  uint32_t index_;
  /// Work stack of nodes.
  std::stack<std::variant<ID<LCSet>, ID<LCDeref>>> stack_;
  /// Components - stored since callbacks mutate the graph.
  std::vector<std::pair<SetGroup, DerefGroup>> sccs_;
};
