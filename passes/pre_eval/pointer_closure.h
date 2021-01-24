// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/adt/bitset.h"
#include "core/adt/hash.h"
#include "core/adt/union_find.h"

class SymbolicContext;
class SymbolicValue;
class SymbolicPointer;
class SymbolicObject;
class SymbolicHeap;
class SymbolicFrame;
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

      Node *operator*() const { return graph_.nodes_.Map(*it_); }
      Node *operator->() const { return operator*(); }

    private:
      PointerClosure &graph_;
      BitSet<Node>::iterator it_;
    };

  public:
    Node(ID<Node> id, PointerClosure &graph) : id_(id), graph_(graph){}

    node_iterator nodes_begin() { return node_iterator(graph_, nodes_.begin()); }
    node_iterator nodes_end() { return node_iterator(graph_, nodes_.end()); }

    /// Merge another node into this one.
    void Union(const Node &that)
    {
      nodes_.Union(that.nodes_);
      self_.Union(that.self_);
      refs_.Union(that.refs_);
      stacks_.Union(that.stacks_);
      funcs_.Union(that.funcs_);
    }

  private:
    friend class PointerClosure;
    /// ID of the node.
    ID<Node> id_;
    /// Reference to the parent graph.
    PointerClosure &graph_;
    /// Referenced nodes.
    BitSet<Node> nodes_;
    /// Set of objects in the node.
    BitSet<SymbolicObject> self_;
    /// Referenced objects, excluding self unless directly referenced.
    BitSet<SymbolicObject> refs_;
    /// Set of referenced stack nodes.
    BitSet<SymbolicFrame> stacks_;
    /// Functions referenced from the nodes.
    BitSet<Func> funcs_;
  };

  /// Iterator over functions.
  using func_iterator = BitSet<Func>::iterator;

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

  /// Add contained objects to the closure.
  void AddRead(Object *g);
  /// Add contained objects to the set of overwritten ones.
  void AddWritten(Object *g);
  /// Add the pointer itself to the closure.
  void AddEscaped(Object *g);

  /// Add a function to the set.
  void Add(Func *f);

  /**
   * Build a pointer containing all the overwritten pointers.
   */
  std::shared_ptr<SymbolicPointer> BuildTainted();

  /**
   * Build a pointer containing all dereferenced pointers.
   */
  std::shared_ptr<SymbolicPointer> BuildTaint();

  /// Return the root node.
  Node *GetRoot() { return nodes_.Map(0); }

  /// Iterator over functions.
  size_t func_size() const { return funcs_.Size(); }
  func_iterator func_begin() const { return funcs_.begin(); }
  func_iterator func_end() const { return funcs_.end(); }
  llvm::iterator_range<func_iterator> funcs() const
  {
    return llvm::make_range(func_begin(), func_end());
  }

private:
  /// Return the node for a static object.
  ID<Node> GetNode(ID<SymbolicObject> id);
  /// Return the node for a static object.
  ID<Node> GetNode(Object *object);

  /// Extract information from an object.
  void Build(ID<Node> id, SymbolicObject &object);

  /// Compact the SCC graph.
  void Compact();

private:
  /// Mapping from objects to IDs.
  SymbolicHeap &heap_;
  /// Heap to operate on.
  SymbolicContext &ctx_;

  /// Allocated heap nodes.
  UnionFind<Node> nodes_;
  /// Mapping from objects to nodes.
  std::unordered_map<ID<SymbolicObject>, ID<Node>> objectToNode_;

  /// Set of objects which have already been built.
  std::set<SymbolicObject *> objects_;
  /// Nodes which are part of the dereferenced items.
  BitSet<SymbolicObject> escapes_;
  /// Nodes which are overwritten.
  BitSet<SymbolicObject> tainted_;
  /// Functions part of the closure.
  BitSet<Func> funcs_;
  /// Stack frames part of the closure.
  BitSet<SymbolicFrame> stacks_;
};
