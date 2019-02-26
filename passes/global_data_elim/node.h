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



// Forward declaration of item types.
class Func;
class Extern;

// Forward declaration of node types.
class RootNode;
class SetNode;
class DerefNode;
class GraphNode;

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
  GraphNode(Kind kind);

  /// Deletes the node.
  virtual ~GraphNode();

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

  /// Adds a function to the set.
  void AddFunc(Func *func) { funcs_.insert(func); }
  /// Adds an extern to the set.
  void AddExtern(Extern *ext) { exts_.insert(ext); }
  /// Adds a node to the set.
  void AddNode(RootNode *node) { nodes_.insert(node); }

  /// Propagates values to another set.
  bool Propagate(SetNode *that);

  /// Replaces the set node with another.
  void Replace(SetNode *that);

  /// Checks if two nodes are equal.
  bool Equals(SetNode *that);

  /// Checks if the node is referenced by roots.
  bool Rooted() const { return !roots_.empty(); }

  /// Adds an edge from this node to another set node.
  bool AddEdge(SetNode *node);
  /// Removes an edge from the graph.
  void RemoveEdge(SetNode *node);

  /// Adds an edge from this node to another set node.
  bool AddEdge(DerefNode *node);
  /// Removes an edge from the graph.
  void RemoveEdge(DerefNode *node);

  /// Iterator over the incoming edges.
  llvm::iterator_range<std::set<SetNode *>::iterator> set_ins()
  {
    return llvm::make_range(setIns_.begin(), setIns_.end());
  }

  /// Iterator over the outgoing edges.
  llvm::iterator_range<std::set<SetNode *>::iterator> set_outs()
  {
    return llvm::make_range(setOuts_.begin(), setOuts_.end());
  }

  /// Checks if there are any incoming set nodes.
  bool set_ins_empty() const { return setIns_.empty(); }
  /// Checks if there are any outgoing set nodes.
  bool set_outs_empty() const { return setIns_.empty(); }

  /// Iterator over the incoming edges.
  llvm::iterator_range<std::set<DerefNode *>::iterator> deref_ins()
  {
    return llvm::make_range(derefIns_.begin(), derefIns_.end());
  }

  /// Iterator over the outgoing edges.
  llvm::iterator_range<std::set<DerefNode *>::iterator> deref_outs()
  {
    return llvm::make_range(derefOuts_.begin(), derefOuts_.end());
  }

  /// Checks if there are any incoming deref nodes.
  bool deref_ins_empty() const { return derefIns_.empty(); }
  /// Checks if there are any outgoing deref nodes.
  bool deref_outs_empty() const { return derefIns_.empty(); }

  /// Functions pointed to.
  llvm::iterator_range<std::set<Func *>::iterator> points_to_func()
  {
    return llvm::make_range(funcs_.begin(), funcs_.end());
  }

  /// Externs pointed to.
  llvm::iterator_range<std::set<Extern *>::iterator> points_to_ext()
  {
    return llvm::make_range(exts_.begin(), exts_.end());
  }

  /// Nodes pointed to.
  llvm::iterator_range<std::set<RootNode *>::iterator> points_to_node()
  {
    return llvm::make_range(nodes_.begin(), nodes_.end());
  }

  /// Root nodes referencing the set.
  llvm::iterator_range<std::set<RootNode *>::iterator> roots()
  {
    return llvm::make_range(roots_.begin(), roots_.end());
  }

private:
  friend class RootNode;
  friend class DerefNode;

  /// Root nodes using the set.
  std::set<RootNode *> roots_;

  /// Incoming set nodes.
  std::set<SetNode *> setIns_;
  /// Outgoing set nodes.
  std::set<SetNode *> setOuts_;
  /// Incoming deref nodes.
  std::set<DerefNode *> derefIns_;
  /// Outgoing deref nodes.
  std::set<DerefNode *> derefOuts_;

  /// Functions stored in the node.
  std::set<Func *> funcs_;
  /// Externs stored in the node.
  std::set<Extern *> exts_;
  /// Nodes stored in the node.
  std::set<RootNode *> nodes_;
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

  /// Adds an edge from this node to another node.
  bool AddEdge(SetNode *node);
  /// Removes an edge from the graph.
  void RemoveEdge(SetNode *node);

  /// Iterator over the incoming edges.
  llvm::iterator_range<std::set<SetNode *>::iterator> set_ins()
  {
    return llvm::make_range(setIns_.begin(), setIns_.end());
  }

  /// Iterator over the outgoing edges.
  llvm::iterator_range<std::set<SetNode *>::iterator> set_outs()
  {
    return llvm::make_range(setOuts_.begin(), setOuts_.end());
  }

private:
  friend class Node;
  friend class SetNode;
  /// Dereferenced node.
  GraphNode *node_;

  /// Incoming nodes.
  std::set<SetNode *> setIns_;
  /// Outgoing nodes.
  std::set<SetNode *> setOuts_;
};

/**
 * Root node. Cannot be deleted.
 */
class RootNode final : public Node {
public:
  /// Creates a new root node.
  RootNode(SetNode *actual);

  /// Returns the set node.
  SetNode *Set() const { return actual_; }

private:
  friend class Node;
  friend class SetNode;
  friend class DerefNode;

  /// Actual node.
  SetNode *actual_;
};
