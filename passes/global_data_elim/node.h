// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <functional>
#include <optional>
#include <unordered_map>
#include <set>
#include <utility>

#include <llvm/ADT/iterator.h>
#include <llvm/ADT/iterator_range.h>



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

  /// Returns a node dereferencing this one.
  Node *Deref();

  /// Adds an edge from this node to another node.
  void AddEdge(Node *node);

  /// Iterator over the outgoing edges.
  llvm::iterator_range<std::set<Node *>::iterator> outs()
  {
    return llvm::make_range(outs_.begin(), outs_.end());
  }

protected:
  /// Creates a new node.
  Node(Kind kind);

private:
  /// Node kind.
  Kind kind_;
  /// Each node should be de-referenced by a unique deref node.
  DerefNode *deref_;
  /// Incoming nodes.
  std::set<Node *> ins_;
  /// Outgoing nodes.
  std::set<Node *> outs_;
};


/**
 * Root node. Cannot be deleted.
 */
class RootNode final : public Node {
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
  /// Constructs a new set node.
  SetNode();
  /// Creates a new node with an item.
  SetNode(uint64_t item);
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
