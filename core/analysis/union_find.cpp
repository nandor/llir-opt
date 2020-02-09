// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/analysis/union_find.h"



// -----------------------------------------------------------------------------
UnionFind::UnionFind(unsigned n)
  : nodes_(n)
{
  for (unsigned i = 0; i < n; ++i) {
    nodes_[i].Class = i;
    nodes_[i].Parent = i;
    nodes_[i].Rank = 0;
  }
}

// -----------------------------------------------------------------------------
void UnionFind::Union(unsigned a, unsigned b)
{
  unsigned idA = FindNode(a);
  unsigned idB = FindNode(b);
  if (idA == idB) {
    return;
  }

  Node &nodeA = nodes_[idA];
  Node &nodeB = nodes_[idB];

  // Update parent info.
  if (nodeA.Rank < nodeB.Rank) {
    nodeA.Parent = b;
  } else {
    nodeB.Parent = a;
  }

  nodeA.Class = b;
  nodeB.Class = b;


  // Update rank if equal.
  if (nodeA.Rank == nodeB.Rank) {
    nodeA.Rank += 1;
  }
}

// -----------------------------------------------------------------------------
unsigned UnionFind::Find(unsigned node)
{
  return nodes_[FindNode(node)].Class;
}

// -----------------------------------------------------------------------------
unsigned UnionFind::FindNode(unsigned node)
{
  unsigned root = node;
  while (nodes_[root].Parent != root) {
    root = nodes_[root].Parent;
  }
  while (nodes_[node].Parent != node) {
    unsigned parent = nodes_[node].Parent;
    nodes_[node].Parent = root;
    node = parent;
  }

  return node;
}
