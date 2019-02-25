// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/iterator.h>
#include <llvm/ADT/iterator_range.h>



// Forward declaration of node types.
class RootNode;
class SetNode;
class DerefNode;


/**
 * Bag of possible nodes.
 */
class Node : public llvm::ilist_node<Node> {
public:
  /// Creates a new node.
  Node();

  /// Creates a new node with an item.
  Node(uint64_t item);

  /// Returns a node dereferencing this one.
  Node *Deref();

  /// Adds an edge from this node to another node.
  void AddEdge(Node *node);

private:
  /// Each node should be de-referenced by a unique deref node.
  DerefNode *deref_;
  /// Incoming nodes.
  llvm::ilist<Node> ins_;
  /// Outgoing nodes.
  llvm::ilist<Node> outs_;
};


/**
 * Root node. Cannot be deleted.
 */
class RootNode final : public Node, public llvm::ilist_node<RootNode> {
public:
  /// Creates a new root node.
  RootNode();
  /// Creates a new root node with an item.
  RootNode(uint64_t item);

private:
  /// Actual node.
  Node *node_;
};


/**
 * Set node in the graph.
 */
class SetNode final : public Node {
public:

private:

};


/**
 * Dereference node in the graph.
 */
class DerefNode final : public Node {
public:
  /// Creates a new node to dereference a value.
  DerefNode(Node *node);

private:
  friend class Node;
  /// Dereferenced node.
  Node *node_;
};
