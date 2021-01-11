// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/adt/bitset.h"

class SymbolicContext;
class SymbolicValue;
class Func;
class Object;



/**
 * Helper to compute the transitive closures of objects on the heap.
 */
class HeapGraph {
public:
  struct Node {

  };

  /**
   * Build a graph of the SCCs of heap nodes.
   */
  HeapGraph(SymbolicContext &ctx);

  /**
   * Transitively extract information pointed to by value.
   *
   * Update value with the newly extracted nodes.
   */
  void Extract(
      SymbolicValue &value,
      std::set<Func *> &funcs,
      BitSet<Node> &nodes
  );

  /**
   * Transitively extract information pointed to by the object.
   *
   * Update value with the newly extracted nodes.
   */
  void Extract(
      Object *g,
      SymbolicValue &value,
      std::set<Func *> &funcs,
      BitSet<Node> &nodes
  );

private:
  /// Heap to operate on.
  SymbolicContext &ctx_;
};
