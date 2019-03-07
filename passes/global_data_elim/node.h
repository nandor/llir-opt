// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <functional>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/iterator.h>
#include <llvm/ADT/iterator_range.h>

#include "passes/global_data_elim/bitset.h"



// Forward declaration of item types.
class Func;
class Extern;

// Forward declaration of node types.
class RootNode;
class SetNode;
class DerefNode;
class GraphNode;

// Solver needs access to traversal fields.
class SCCSolver;

class ConstraintSolver;

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

  /// Converts the node to a root node (if it is one).
  RootNode *AsRoot();

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
  GraphNode(Kind kind, uint64_t id);

  /// Deletes the node.
  virtual ~GraphNode();

  /// Returns the ID of the node.
  uint64_t GetID() const { return id_; }

  /// Returns a node dereferencing this one.
  DerefNode *Deref();

  /// Checks if the node is a load.
  bool IsDeref() const { return kind_ == Kind::DEREF; }
  /// Checks if the ndoe is a set.
  bool IsSet() const { return kind_ == Kind::SET; }

  /// Returns the node as a set, if it is one.
  SetNode *AsSet();
  /// Returns the node as a deref, if it is one.
  DerefNode *AsDeref();

protected:
  friend class SetNode;
  friend class DerefNode;
  /// Each node should be de-referenced by a unique deref node.
  DerefNode *deref_;
  /// ID of the node.
  uint64_t id_;

private:
  /// Solver needs access.
  friend class SCCSolver;
  friend class ConstraintSolver;
  /// Epoch the node was visited in.
  uint32_t Epoch;
  /// Index on the stack.
  uint32_t Index;
  /// Lowest link.
  uint32_t Link;
  /// Flag to indicate if node on stack.
  bool InComponent;
};

/**
 * Set node in the graph.
 */
class SetNode final : public GraphNode {
public:
  /// Constructs a new set node.
  SetNode(uint64_t id);

  /// Deletes a set node.
  ~SetNode();

  /// Adds a function to the set.
  void AddFunc(BitSet<Func *>::Item func) { funcs_.Insert(func); }
  /// Adds an extern to the set.
  void AddExtern(BitSet<Extern *>::Item ext) { exts_.Insert(ext); }
  /// Adds a node to the set.
  void AddNode(BitSet<SetNode *>::Item node) { nodes_.Insert(node); }

  /// Updates a node ID.
  void UpdateNode(uint32_t from, uint32_t to);

  /// Propagates values to another set.
  bool Propagate(SetNode *that);

  /// Replaces the set node with another.
  void Replace(
      const std::vector<SetNode *> &sets,
      const std::vector<DerefNode *> &derefs,
      SetNode *that
  );

  /// Checks if two nodes are equal.
  bool Equals(SetNode *that);

  /// Adds an edge from this node to another set node.
  bool AddSet(SetNode *node);
  /// Updates a set after collapsing.
  void UpdateSet(uint32_t from, uint32_t to);

  /// Adds an edge from this node to another set node.
  bool AddDeref(DerefNode *node);
  /// Removes an edge from the graph.
  void RemoveDeref(DerefNode *node);

  /// Iterator over the outgoing edges.
  llvm::iterator_range<BitSet<SetNode *>::iterator> sets()
  {
    return llvm::make_range(sets_.begin(), sets_.end());
  }

  /// Iterator over the outgoing edges.
  llvm::iterator_range<BitSet<DerefNode *>::iterator> derefs()
  {
    return llvm::make_range(derefOuts_.begin(), derefOuts_.end());
  }

  /// Functions pointed to.
  llvm::iterator_range<BitSet<Func *>::iterator> points_to_func()
  {
    return llvm::make_range(funcs_.begin(), funcs_.end());
  }

  /// Externs pointed to.
  llvm::iterator_range<BitSet<Extern *>::iterator> points_to_ext()
  {
    return llvm::make_range(exts_.begin(), exts_.end());
  }

  /// Nodes pointed to.
  llvm::iterator_range<BitSet<SetNode *>::iterator> points_to_node()
  {
    return llvm::make_range(nodes_.begin(), nodes_.end());
  }

private:
  friend class RootNode;
  friend class DerefNode;

  /// Outgoing set nodes.
  BitSet<SetNode *> sets_;
  /// Incoming deref nodes.
  BitSet<DerefNode *> derefIns_;
  /// Outgoing deref nodes.
  BitSet<DerefNode *> derefOuts_;

  /// Functions stored in the node.
  BitSet<Func *> funcs_;
  /// Externs stored in the node.
  BitSet<Extern *> exts_;
  /// Nodes stored in the node.
  BitSet<SetNode *> nodes_;
};

/**
 * Dereference node in the graph.
 */
class DerefNode final : public GraphNode {
public:
  /// Creates a new node to dereference a value.
  DerefNode(SetNode *node, RootNode *contents, uint64_t id);

  /// Deletes the deref node.
  ~DerefNode();

  /// Replaces the set node with another.
  void Replace(const std::vector<SetNode *> &sets, DerefNode *that);

  /// Returns the dereferenced node.
  SetNode *Node() const { return node_; }

  /// Returns the set node with the contents.
  SetNode *Contents();

  /// Adds an edge from this node to another node.
  bool AddSet(SetNode *node);
  /// Removes an edge from the graph.
  void RemoveSet(SetNode *node);

  /// Iterator over the incoming edges.
  llvm::iterator_range<BitSet<SetNode *>::iterator> set_ins()
  {
    return llvm::make_range(setIns_.begin(), setIns_.end());
  }

  /// Iterator over the outgoing edges.
  llvm::iterator_range<BitSet<SetNode *>::iterator> set_outs()
  {
    return llvm::make_range(setOuts_.begin(), setOuts_.end());
  }

private:
  friend class Node;
  friend class SetNode;

  /// Dereferenced node.
  SetNode *node_;
  /// Loaded contents.
  RootNode *contents_;

  /// Incoming nodes.
  BitSet<SetNode *> setIns_;
  /// Outgoing nodes.
  BitSet<SetNode *> setOuts_;
};

/**
 * Root node. Cannot be deleted.
 */
class RootNode : public Node {
public:
  /// Creates a new root node.
  RootNode(ConstraintSolver *solver, SetNode *actual);

  /// Returns the set node.
  SetNode *Set() const;

private:
  friend class Node;
  friend class SetNode;
  friend class DerefNode;

  /// Reference to the solver.
  ConstraintSolver *solver_;
  /// Actual node ID.
  mutable uint32_t id_;
};

