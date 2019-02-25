// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_map>
#include <unordered_set>

#include <llvm/ADT/ilist.h>

class Node;
class RootNode;



// -----------------------------------------------------------------------------
class ConstraintSolver final {
public:
  /// Arguments & return values to a function.
  struct FuncSet {
    /// Argument sets.
    std::vector<Node *> Args;
    /// Return set.
    Node *Return;
    /// Frame of the function.
    Node *Frame;
    /// Variable argument glob.
    Node *VA;
    /// True if function was expanded.
    bool Expanded;
  };

public:
  /// Initialises the solver.
  ConstraintSolver();

  /// Returns a load constraint.
  Node *Load(Node *ptr);

  /// Generates a subset constraint.
  void Subset(Node *from, Node *to);

  /// Constructs a root node.
  RootNode *Root();

  /// Constructs a root node, with a single function.
  RootNode *Root(RootNode *node);

  /// Constructs a root node, with a single node.
  RootNode *Root(Func *func);

  /// Constructs a root node, with a single node.
  RootNode *Root(Extern *ext);

  /// Constructs an empty node.
  Node *Empty();

  /// Constructs a root node for an atom.
  RootNode *Chunk(Atom *atom, RootNode *chunk);

public:
  /// Creates a store constraint.
  void Store(Node *ptr, Node *val)
  {
    Subset(val, Load(ptr));
  }

  /// Returns a binary set union.
  Node *Union(Node *a, Node *b)
  {
    if (!a) {
      return b;
    }
    if (!b) {
      return a;
    }

    auto *node = Empty();
    Subset(a, node);
    Subset(b, node);
    return node;
  }

  /// Returns a ternary set union.
  Node *Union(Node *a, Node *b, Node *c)
  {
    return Union(a, Union(b, c));
  }

  /// Indirect call, to be expanded.
  Node *Call(
      const std::vector<Inst *> &context,
      Node *callee,
      std::vector<Node *> args)
  {
    auto *ret = Root();
    calls_.emplace_back(context, callee, args, ret);
    return ret;
  }

  /// Allocation site.
  Node *Alloc(const std::vector<Inst *> &context)
  {
    return Empty();
  }

  /// Extern function context.
  Node *External()
  {
    return extern_;
  }

  /// Returns the node attached to a global.
  Node *Lookup(Global *global);

  /// Returns the constraints attached to a function.
  FuncSet &Lookup(const std::vector<Inst *> &calls, Func *func);

  /// Simplifies the constraints.
  void Progress();

  /// Simplifies the whole batch.
  std::vector<std::pair<std::vector<Inst *>, Func *>> Expand();

private:
  /// Creates a root node with an item.
  RootNode *Root(uint64_t item);
  /// Maps a function to a bitset ID.
  uint64_t Map(Func *func);
  /// Maps an extern to a bitset ID.
  uint64_t Map(Extern *ext);
  /// Maps a node to a bitset ID.
  uint64_t Map(RootNode *node);
  /// Creates a node.
  template<typename T, typename... Args>
  T *Make(Args... args);

private:
  /// Call site information.
  struct CallSite {
    /// Call context.
    std::vector<Inst *> Context;
    /// Called function.
    Node *Callee;
    /// Arguments to call.
    std::vector<Node *> Args;
    /// Return value.
    Node *Return;
    /// Expanded callees at this site.
    std::set<Func *> Expanded;

    CallSite(
        const std::vector<Inst *> &context,
        Node *callee,
        std::vector<Node *> args,
        Node *ret)
      : Context(context)
      , Callee(callee)
      , Args(args)
      , Return(ret)
    {
    }
  };

  /// Mapping from functions to IDs.
  std::unordered_map<Func *, uint64_t> funcIDs_;
  /// Mapping from externs to IDs.
  std::unordered_map<Extern *, uint64_t> extIDs_;
  /// Mapping from roots to IDs.
  std::unordered_map<Node *, uint64_t> rootIDs_;

  /// List of root nodes.
  llvm::ilist<RootNode> roots_;

  /// Function argument/return constraints.
  std::map<Func *, std::unique_ptr<FuncSet>> funcs_;
  /// Global variables.
  std::unordered_map<Global *, RootNode *> globals_;
  /// Node representing external values.
  Node *extern_;
  /// Call sites.
  std::vector<CallSite> calls_;
};
