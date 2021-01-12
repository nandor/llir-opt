// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/adt/bitset.h"
#include "core/adt/hash.h"

class SymbolicContext;
class SymbolicValue;
class SymbolicObject;
class Func;
class Object;
class CallSite;



/**
 * Helper to compute the transitive closures of objects on the heap.
 */
class HeapGraph {
public:
  class Node {
  public:
    /**
     * Iterator over referenced nodes.
     */
    class node_iterator : public std::iterator
      < std::forward_iterator_tag
      , Node *
      >
    {
    public:
      node_iterator(HeapGraph &graph, BitSet<Node>::iterator it)
        : graph_(graph)
        , it_(it)
      {
      }

      bool operator==(const node_iterator &that) const { return it_ == that.it_; }
      bool operator!=(const node_iterator &that) const { return !(*this == that); }

      node_iterator &operator++()
      {
        ++it_;
        return *this;
      }

      node_iterator operator++(int)
      {
        auto tmp = *this;
        ++*this;
        return tmp;
      }

      Node *operator*() const { return &graph_.nodes_[*it_]; }
      Node *operator->() const { return operator*(); }

    private:
      HeapGraph &graph_;
      BitSet<Node>::iterator it_;
    };

  public:
    Node(HeapGraph &graph) : graph_(graph) {}

    node_iterator nodes_begin() { return node_iterator(graph_, nodes_.begin()); }
    node_iterator nodes_end() { return node_iterator(graph_, nodes_.end()); }

  private:
    friend class HeapGraph;
    HeapGraph &graph_;
    BitSet<Node> nodes_;
    std::set<Func *> funcs_;
    std::set<unsigned> stacks_;
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
      const SymbolicValue &value,
      std::set<Func *> &funcs,
      std::set<unsigned> &stacks,
      BitSet<Node> &nodes
  );

  /**
   * Transitively extract information pointed to by the object.
   *
   * Update value with the newly extracted nodes.
   */
  void Extract(
      Object *g,
      std::set<Func *> &funcs,
      BitSet<Node> &nodes
  );

  /// Build a pointer containing all the visited items.
  SymbolicValue Build(
      const std::set<Func *> &funcs,
      const std::set<unsigned> &stacks,
      const BitSet<Node> &nodes
  );

  /// Return the root node.
  Node *GetRoot() { return &nodes_[0]; }

private:
  /// Return the node for an static object.
  ID<Node> GetNode(Object *id);
  /// Return the node for a frame object.
  ID<Node> GetNode(const std::pair<unsigned, unsigned> &id);
  /// Return the node for a call site.
  ID<Node> GetNode(const std::pair<unsigned, CallSite *> &id);

  /// Extract information from an object.
  void Build(ID<Node> id, SymbolicObject &object);

private:
  /// Heap to operate on.
  SymbolicContext &ctx_;

  /// Allocated heap nodes.
  std::vector<Node> nodes_;
  /// Mapping from objects to nodes.
  std::unordered_map<Object *, ID<Node>> objectToNode_;
  /// Mapping from frames to nodes.
  std::unordered_map<std::pair<unsigned, unsigned>, ID<Node>> frameToNode_;
  /// Mapping from allocations to nodes.
  std::unordered_map<std::pair<unsigned, CallSite *>, ID<Node>> allocToNode_;
};
