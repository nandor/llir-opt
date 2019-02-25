// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <functional>
#include <optional>
#include <unordered_map>
#include <set>
#include <utility>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/iterator.h>
#include <llvm/ADT/iterator_range.h>

#include "passes/global_data_elim/scc.h"



// Forward declaration of node types.
class RootNode;
class SetNode;
class DerefNode;



/**
 * Bag of possible nodes.
 */
class Node {
public:
  /// Enumeration of enum kinds.
  enum class Kind {
    SET,
    DEREF,
    ROOT,
  };

  /// Virtual destructor.
  virtual ~Node();

  /// Converts the node to a graph node.
  GraphNode *ToGraph();

protected:
  /// Creates a new node.
  Node(Kind kind);

protected:
  /// Node kind.
  Kind kind_;
};

/**
 * Actual node in the graph, i.e. not a root node.
 */
class GraphNode : public Node {
public:
  /// Constructs a graph node.
  GraphNode(Kind kind);

  /// Deletes the node.
  virtual ~GraphNode();

  /// Returns a node dereferencing this one.
  DerefNode *Deref();

  /// Adds an edge from this node to another node.
  void AddEdge(GraphNode *node);
  /// Removes an edge from the graph.
  void RemoveEdge(GraphNode *node);

  /// Iterator over the outgoing edges.
  llvm::iterator_range<std::set<GraphNode *>::iterator> ins()
  {
    return llvm::make_range(ins_.begin(), ins_.end());
  }

  /// Iterator over the outgoing edges.
  llvm::iterator_range<std::set<GraphNode *>::iterator> outs()
  {
    return llvm::make_range(outs_.begin(), outs_.end());
  }

  /// Checks if the node is a load.
  bool IsDeref() const { return kind_ == Kind::DEREF; }
  /// Checks if the ndoe is a set.
  bool IsSet() const { return kind_ == Kind::SET; }

  /// Checks if the node can be converted to a set.
  SetNode *AsSet();

protected:
  friend class SetNode;
  friend class DerefNode;
  /// Each node should be de-referenced by a unique deref node.
  DerefNode *deref_;
  /// Incoming nodes.
  std::set<GraphNode *> ins_;
  /// Outgoing nodes.
  std::set<GraphNode *> outs_;

private:
  /// Solver needs access.
  friend class SCCSolver;
  /// Index on the stack.
  uint32_t Index;
  /// Lowest link.
  uint32_t Link;
  /// Flag to indicate if node on stack.
  bool OnStack;
};

/**
 * Set node in the graph.
 */
class SetNode final : public GraphNode {
public:
  /// Constructs a new set node.
  SetNode();
  /// Creates a new node with an item.
  SetNode(uint64_t item);

  /// Propagates values to another set.
  bool Propagate(SetNode *that);

  /// Replaces the set node with another.
  void Replace(SetNode *that);

  /// Checks if the node is referenced by roots.
  bool Rooted() const { return !roots_.empty(); }

private:
  friend class RootNode;
  /// Root nodes using the set.
  llvm::ilist<RootNode> roots_;

  /// Values stored in the node.
  std::set<uint64_t> items_;
};

/**
 * Dereference node in the graph.
 */
class DerefNode final : public GraphNode {
public:
  /// Creates a new node to dereference a value.
  DerefNode(GraphNode *node);

  /// Replaces the set node with another.
  void Replace(DerefNode *that);

private:
  friend class Node;
  friend class SetNode;
  /// Dereferenced node.
  GraphNode *node_;
};

/**
 * Root node. Cannot be deleted.
 */
class RootNode final : public Node, public llvm::ilist_node<RootNode> {
public:
  /// Creates a new root node.
  RootNode(SetNode *actual);

private:
  friend class Node;
  friend class SetNode;
  friend class DerefNode;
  /// Actual node.
  SetNode *actual_;
};
