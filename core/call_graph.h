// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/PointerUnion.h>

class Prog;
class Func;
class Inst;



/**
 * Lazily built call graph.
 */
class CallGraph final {
public:
  /**
   * Node in the lazy call graph.
   */
  class Node {
  public:
    /// Iterator over call site targets.
    class iterator {
    public:
      /// Start iterator.
      iterator(const Node *node, Inst *start);
      /// Start iterator.
      iterator(const Node *node, Func *func);
      /// End iterator.
      iterator() : it_(static_cast<Inst *>(nullptr)) {}

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
      llvm::PointerUnion<Inst *, Func *> it_;
    };

  public:

    /// Entry node.
    Node(const CallGraph *graph, Prog *prog);
    /// Internal graph node.
    Node(const CallGraph *graph, Func *caller);

    /// Return iterators over the callees.
    iterator begin() const;
    iterator end() const { return iterator(); }

    /// Returns the function, null for entry.
    Func *GetCaller() const;

  private:
    friend class iterator;
    /// Parent graph.
    const CallGraph *graph_;
    /// Caller or null for the entry node.
    llvm::PointerUnion<Func *, Prog *> node_;
  };

public:
  /// Creates a call graph for a program.
  CallGraph(Prog &p);

  /// Cleanup.
  ~CallGraph();

  /// Returns the virtual the entry node.
  const Node *Entry() const { return &entry_; }

  /// Returns the node for a function.
  Node *operator[](Func *f) const;

private:
  friend class Node::iterator;
  /// Virtual entry node, linking to main or functions with address taken.
  Node entry_;
  /// Mapping from functions to their cached nodes.
  mutable std::unordered_map<Func *, std::unique_ptr<Node>> nodes_;
};

/// Graph traits for call graph nodes.
namespace llvm {

template <>
struct GraphTraits<CallGraph::Node *> {
  using NodeRef = const CallGraph::Node *;
  using ChildIteratorType = CallGraph::Node::iterator;

  static ChildIteratorType child_begin(NodeRef N) { return N->begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->end(); }
};

/// Graph traits for the call graph.
template <>
struct GraphTraits<CallGraph *> : public GraphTraits<CallGraph::Node *> {
  static NodeRef getEntryNode(const CallGraph *g)
  {
    return g->Entry();
  }
};

} // end namespace llvm
