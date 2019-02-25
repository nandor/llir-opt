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

  /// Returns a node dereferencing this one.
  virtual Node *Deref();

  /// Adds an edge from this node to another node.
  virtual void AddEdge(Node *node);

  /// Iterator over the outgoing edges.
  virtual llvm::iterator_range<std::set<Node *>::iterator> outs()
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
 * Root node. Cannot be deleted.
 */
class RootNode final : public Node {
public:
  /// Creates a new root node.
  RootNode();
  /// Creates a new root node with an item.
  RootNode(uint64_t item);

  /// Forward to node.
  Node *Deref() override
  {
    return node_->Deref();
  }

  /// Forward to node.
  void AddEdge(Node *node) override
  {
    return node_->AddEdge(node);
  }

  /// Forward to node.
  llvm::iterator_range<std::set<Node *>::iterator> outs() override
  {
    return node_->outs();
  }

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
