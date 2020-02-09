// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>
#include <vector>



/**
 * Union-Find data structure.
 */
class UnionFind {
public:
  /// Creates a new structure with a given number of sets.
  UnionFind(unsigned n);

  /// Joins two nodes, using the second one as the representative node.
  void Union(unsigned a, unsigned b);

  /// Finds the representative class of a node.
  unsigned Find(unsigned node);

private:
  /// A tree node.
  struct Node {
    /// Parent node link.
    unsigned Parent;
    /// Representative class.
    unsigned Class;
    /// Node rank.
    unsigned Rank;
  };

  /// Finds the node of an item.
  unsigned FindNode(unsigned node);

  /// List of nodes.
  std::vector<Node> nodes_;
};
