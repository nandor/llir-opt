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
class SymbolicHeap;
class Func;
class Object;
class CallSite;



/**
 * Helper to compute the transitive closures of objects on the heap.
 */
class PointerClosure {
public:
  /**
   * Node in the graph.
   */
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
      node_iterator(PointerClosure &graph, BitSet<Node>::iterator it)
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
      PointerClosure &graph_;
      BitSet<Node>::iterator it_;
    };

  public:
    Node(PointerClosure &graph) : graph_(graph) {}

    node_iterator nodes_begin() { return node_iterator(graph_, nodes_.begin()); }
    node_iterator nodes_end() { return node_iterator(graph_, nodes_.end()); }

  private:
    friend class PointerClosure;
    PointerClosure &graph_;
    BitSet<Node> nodes_;
    std::set<Func *> funcs_;
    std::set<unsigned> stacks_;
  };

  /// Iterator over functions.
  using func_iterator = std::set<Func *>::const_iterator;

public:
  /**
   * Build a graph of the SCCs of heap nodes.
   */
  PointerClosure(SymbolicHeap &heap, SymbolicContext &ctx);

  /**
   * Transitively extract information pointed to by value.
   *
   * Update value with the newly extracted nodes.
   */
  void Add(const SymbolicValue &value);

  /**
   * Transitively extract information pointed to by the object.
   *
   * Update value with the newly extracted nodes.
   */
  void Add(Object *g);

  /**
   * Add a function to the set.
   */
  void Add(Func *f);

  /**
   * Build a pointer containing all the visited items.
   */
  SymbolicValue Build();

  /// Return the root node.
  Node *GetRoot() { return &nodes_[0]; }

  /// Iterator over functions.
  size_t func_size() const { return std::distance(func_begin(), func_end()); }
  func_iterator func_begin() const { return funcs_.begin(); }
  func_iterator func_end() const { return funcs_.end(); }
  llvm::iterator_range<func_iterator> funcs() const
  {
    return llvm::make_range(func_begin(), func_end());
  }

private:
  /// Return the node for an static object.
  ID<Node> GetNode(ID<SymbolicObject> id);

  /// Extract information from an object.
  void Build(ID<Node> id, SymbolicObject &object);

private:
  /// Mapping from objects to IDs.
  SymbolicHeap &heap_;
  /// Heap to operate on.
  SymbolicContext &ctx_;

  /// Allocated heap nodes.
  std::vector<Node> nodes_;
  /// Mapping from objects to nodes.
  std::unordered_map<ID<SymbolicObject>, ID<Node>> objectToNode_;

  /// Nodes which are part of the closure.
  BitSet<Node> closure_;
  /// Functions part of the closure.
  std::set<Func *> funcs_;
  /// Stack frames part of the closure.
  std::set<unsigned> stacks_;
};
