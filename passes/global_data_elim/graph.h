// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>
#include <memory>

#include "passes/global_data_elim/id.h"

class Node;
class DerefNode;
class GraphNode;
class RootNode;
class SetNode;



/**
 * Class storing and representing the graph.
 */
class Graph final {
public:
  /// Creates a set node.
  SetNode *Set();
  /// Creates a deref node.
  DerefNode *Deref(SetNode *set);
  /// Creates a root node.
  RootNode *Root(SetNode *set);

  /// Returns a set mapped to an id.
  SetNode *Get(ID<SetNode *> id) const { return sets_[id]; }
  /// Finds a node, given its ID.
  SetNode *Find(ID<SetNode *> id);
  /// Unifies two nodes.
  SetNode *Union(SetNode *a, SetNode *b);

private:
  friend class SCCSolver;

  /// List of all set nodes.
  std::vector<SetNode *> sets_;
  /// List of all deref nodes.
  std::vector<DerefNode *> derefs_;
  /// List of root nodes.
  std::vector<RootNode *> roots_;

  /// All allocated nodes.
  std::vector<std::unique_ptr<Node>> nodes_;

  /// Union-Find information.
  struct Entry {
    /// Parent node ID.
    uint32_t Parent;
    /// Node rank.
    uint32_t Rank;

    /// Creates a new entry.
    Entry(uint32_t parent, uint32_t rank) : Parent(parent), Rank(rank) {}
  };

  /// Union-Find nodes.
  std::vector<Entry> unions_;
};
