// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/PointerUnion.h>
#include <llvm/Support/DOTGraphTraits.h>

#include "core/func.h"
#include "core/inst.h"
#include "core/prog.h"



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
    class iterator : public std::iterator<std::forward_iterator_tag, const Node *> {
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

      const Node *operator*() const;

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

    /// Checks if the node is a tail-recursive function.
    bool IsRecursive() const;

  private:
    friend class iterator;
    /// Parent graph.
    const CallGraph *graph_;
    /// Caller or null for the entry node.
    llvm::PointerUnion<Func *, Prog *> node_;
  };

private:
  using NodeMap = std::unordered_map<const Func *, std::unique_ptr<Node>>;

public:
  /// Iterator over the nodes of the call graph.
  struct const_node_iterator : llvm::iterator_adaptor_base
      < const_node_iterator
      , NodeMap::const_iterator
      , std::random_access_iterator_tag
      , const Node *
      >
  {
    explicit const_node_iterator(NodeMap::const_iterator it)
      : iterator_adaptor_base(it)
    {
    }

    const Node *operator*() const { return I->second.get(); }
    const Node *operator->() const { return I->second.get(); }
  };

public:
  /// Creates a call graph for a program.
  CallGraph(Prog &p);

  /// Cleanup.
  ~CallGraph();

  /// Returns the virtual the entry node.
  const Node *Entry() const { return &entry_; }
  /// Returns the node for a function.
  const Node *operator[](Func *f) const;

  /// Iterator over nodes.
  const_node_iterator begin() const { return const_node_iterator(nodes_.begin()); }
  const_node_iterator end() const { return const_node_iterator(nodes_.end()); }

private:
  friend class Node::iterator;
  /// Virtual entry node, linking to main or functions with address taken.
  Node entry_;
  /// Mapping from functions to their cached nodes.
  mutable NodeMap nodes_;
};

/// Graph traits for call graph nodes.
namespace llvm {

template <>
struct GraphTraits<CallGraph::Node *> {
  using NodeRef = const CallGraph::Node *;

  using ChildIteratorType = CallGraph::Node::iterator;
  static ChildIteratorType child_begin(NodeRef n) { return n->begin(); }
  static ChildIteratorType child_end(NodeRef n) { return n->end(); }
};

/// Graph traits for the call graph.
template <>
struct GraphTraits<CallGraph *> : public GraphTraits<CallGraph::Node *> {
  static NodeRef getEntryNode(const CallGraph *g) { return g->Entry(); }

  using nodes_iterator = CallGraph::const_node_iterator;
  static nodes_iterator nodes_begin(CallGraph *g) { return g->begin(); }
  static nodes_iterator nodes_end(CallGraph *g) { return g->end(); }
};

template<>
struct DOTGraphTraits<CallGraph*> : public DefaultDOTGraphTraits {
  DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

  static std::string getNodeLabel(
      const CallGraph::Node *n,
      const CallGraph *g)
  {
    if (auto *f = n->GetCaller()) {
      return std::string(f->GetName());
    } else {
      return "root";
    }
  }
};

}
