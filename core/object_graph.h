// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/PointerUnion.h>

#include "core/data.h"
#include "core/object.h"
#include "core/atom.h"



/**
 * Lazily built graph of data items.
 */
class ObjectGraph final {
public:
  /**
   * Node in the lazy object graph.
   */
  class Node {
  public:
    /// Iterator over object references.
    class iterator {
    public:
      /// Start iterator.
      iterator(const Node *node, const Item *start);
      /// Start iterator.
      iterator(const Node *node, const Object *func);
      /// End iterator.
      iterator() : it_(static_cast<Item *>(nullptr)) {}

      bool operator!=(const iterator &that) const { return !(*this == that); }
      bool operator==(const iterator &that) const;

      iterator &operator++();

      iterator operator++(int)
      {
        iterator it(*this);
        ++*this;
        return it;
      }

      Node *operator*() const;

    private:
      /// Parent node.
      const Node *node_;
      /// Current instruction.
      llvm::PointerUnion<const Item *, const Object *> it_;
    };

  public:

    /// Entry node.
    Node(const ObjectGraph *graph, const Prog *prog);
    /// Internal graph node.
    Node(const ObjectGraph *graph, const Object *object);

    /// Return iterators over the referenced objects.
    iterator begin() const;
    iterator end() const { return iterator(); }

    /// Returns the object, null for virtual entry.
    const Object *GetObject() const;

  private:
    friend class iterator;
    /// Parent graph.
    const ObjectGraph *graph_;
    /// Caller or null for the entry node.
    llvm::PointerUnion<const Object *, const Prog *> node_;
  };

public:
  /// Creates a object graph for a program.
  ObjectGraph(const Prog &p);

  /// Cleanup.
  ~ObjectGraph();

  /// Returns the virtual the entry node.
  const Node *Entry() const { return &entry_; }

  /// Returns the node for an object.
  Node *operator[](const Object *o) const;

private:
  friend class Node::iterator;
  /// Virtual entry node, linking to all objects referenced by code.
  Node entry_;
  /// Mapping from objects to their cached nodes.
  mutable std::unordered_map<const Object *, std::unique_ptr<Node>> nodes_;
};

/// Graph traits for object graph nodes.
namespace llvm {

template <>
struct GraphTraits<ObjectGraph::Node *> {
  using NodeRef = const ObjectGraph::Node *;
  using ChildIteratorType = ObjectGraph::Node::iterator;

  static ChildIteratorType child_begin(NodeRef N) { return N->begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->end(); }
};

/// Graph traits for the object graph.
template <>
struct GraphTraits<ObjectGraph *> : public GraphTraits<ObjectGraph::Node *> {
  static NodeRef getEntryNode(const ObjectGraph *g)
  {
    return g->Entry();
  }
};

} // end namespace llvm
