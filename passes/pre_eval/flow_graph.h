// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/adt/id.h"
#include "core/adt/bitset.h"
#include "core/adt/union_find.h"
#include "core/insts.h"

class Prog;
class Func;
class Object;
class Block;



/**
 * Simplified block-level graph containing blocks/instructions.
 */
class FlowGraph final {
public:
  /// Flow graph node.
  struct Node {
    /// Link to a function called from the node.
    const Func *Callee;

    /// Set of referenced functions.
    BitSet<Func> Funcs;
    /// Set of referenced blocks.
    BitSet<Block> Blocks;
    /// Set of referenced objects.
    BitSet<Object> Objects;
    /// Set of blocks represented by the node.
    BitSet<Inst> Origins;
    /// Flag indicating the presence of indirect jumps.
    bool HasIndirectJumps;
    /// Flag indicating the presence of indirect calls.
    bool HasIndirectCalls;
    /// Flag indicating whether the node is a loop.
    bool IsLoop;
    /// Flag indicating whether the node is an exit node.
    bool IsExit;

    /// Successor nodes.
    BitSet<Node> Successors;
  };

public:
  /// Initialises the flow graph for a program.
  FlowGraph(Prog &prog);

  /// Maps an object ID to an object.
  const Object *operator[](ID<Object> id) const { return objectMap_[id]; }
  /// Maps an function ID to a function.
  const Func *operator[](ID<Func> id) const { return funcMap_[id]; }
  /// Maps a block ID to a block.
  const Block *operator[](ID<Block> id) const { return blockMap_[id]; }
  /// Maps an instruction ID to an instruction.
  const Inst *operator[](ID<Inst> id) const { return instMap_[id]; }

  /// Maps a function to its flow graph node.
  ID<Node> operator[](const Func *func);
  /// Maps a block to its node.
  const FlowGraph::Node *operator[](const Block *block)
  {
    auto it = blocks_.find(&*block->begin());
    assert(it != blocks_.end() && "missing block");
    return this->operator[](it->second);
  }
  /// Map a node ID to a node.
  const FlowGraph::Node *operator[](ID<Node> id)
  {
    assert(id < nodes_.size() && "missing node");
    return &nodes_[id];
  }

private:
  /// Mapping from objects to IDs.
  template<typename T>
  class ObjectToID {
  public:
    ID<T> operator[](const T *t)
    {
      if (auto it = objToID_.find(t); it != objToID_.end()) {
        return it->second;
      }
      auto id = ID<T>(idToObj_.size());
      idToObj_.push_back(t);
      objToID_.emplace(t, id);
      return id;
    }

    const T *operator[](ID<T> id) const { return idToObj_[id]; }

    size_t Size() const { return idToObj_.size(); }

  private:
    /// Mapping from pointers to IDs.
    std::unordered_map<const T *, ID<T>> objToID_;
    /// Mapping from IDs to pointers.
    std::vector<const T *> idToObj_;
  };

  /// Mapping between blocks and IDs.
  ObjectToID<Block> blockMap_;
  /// Mapping between functions and IDs.
  ObjectToID<Func> funcMap_;
  /// Mapping between objects and IDs.
  ObjectToID<Object> objectMap_;
  /// Mapping between instructions and IDs.
  ObjectToID<Inst> instMap_;

private:
  /// Set of objects/items referenced transitively by an object.
  struct ObjectRefs {
    /// Set of referenced functions.
    BitSet<Func> Funcs;
    /// Set of referenced blocks.
    BitSet<Block> Blocks;
    /// Set of referenced objects.
    BitSet<Object> Objects;
  };
  /// Mapping from objects to references.
  std::unordered_map<const Object *, std::shared_ptr<ObjectRefs>> objRefs_;

  /// Set of objects/items referenced transitively by a function and callees.
  struct FunctionRefs {
    /// Set of referenced functions.
    BitSet<Func> Funcs;
    /// Set of referenced blocks.
    BitSet<Block> Blocks;
    /// Set of referenced objects.
    BitSet<Object> Objects;
    /// Flag indicating the presence of indirect jumps.
    bool HasIndirectJumps = false;
    /// Flag indicating the presence of indirect calls.
    bool HasIndirectCalls = false;
  };
  /// Mapping from functions to references.
  std::unordered_map<const Func *, std::shared_ptr<FunctionRefs>> funcRefs_;

  /// Extracts references from an instruction.
  void ExtractRefs(const Inst &inst, FunctionRefs &refs);
  /// Extracts references from a move instruction.
  void ExtractRefsMove(const MovInst &inst, FunctionRefs &refs);
  /// Extracts references from an atom.
  void ExtractRefsAtom(const Atom *atom, FunctionRefs &refs);
  /// Extracts references from a call instruction.
  void ExtractRefsCallee(const Inst *callee, FunctionRefs &refs);
  /// Extracts references from a call instruction.
  const Func *BuildCallRefs(const Inst *callee, FunctionRefs &refs);

  /// Nodes in the graph.
  std::vector<Node> nodes_;
  /// Function entry and exit points.
  std::unordered_map<const Func *, ID<Node>> funcs_;
  /// Blocks which have their address taken.
  std::unordered_map<const Inst *, ID<Node>> blocks_;
  /// Creates a loop node.
  ID<Node> CreateNode(
      FunctionRefs &&refs,
      BitSet<Inst> origins,
      const Func *callee,
      bool IsLoop,
      bool IsExit
  );

  /// Build a node for a function.
  void BuildNode(const Func &func);
  /// Build a node for a loop.
  void BuildLoop(const std::set<const Func *> &funcs);
};
