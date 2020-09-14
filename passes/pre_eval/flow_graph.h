// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include "core/adt/id.h"
#include "core/adt/bitset.h"
#include "core/adt/union_find.h"

class Prog;
class Func;
class Object;
class Block;



/**
 * Simplified block-level graph containing blocks/instructions.
 */
class FlowGraph final {
public:
  /// Initialises the flow graph for a program.
  FlowGraph(Prog &prog);

private:
  /// Graph node.
  struct RawNode {
    /// Unique node ID.
    ID<RawNode> NodeID;
    /// The node contains an indirect jump.
    bool HasIndirectJump;
    /// The node contains an indirect call.
    bool HasIndirectCall;
    /// The node cannot be erased from the graph.
    bool Anchor;
    /// Function entries this node represents.
    std::set<Func *> Entries;
    /// Referenced functions.
    std::set<Func *> Funcs;
    /// Referenced objects.
    std::set<Object *> Objects;
    /// Referenced blocks.
    std::set<Block *> Blocks;
    /// Successor nodes.
    BitSet<RawNode> Successors;

    /// Initialises the node.
    RawNode(ID<RawNode> nodeID, Func *f)
      : NodeID(nodeID)
      , HasIndirectJump(false)
      , HasIndirectCall(false)
      , Anchor(false)
      , Entries({ f })
    {
    }

    /// Unifies two nodes.
    bool Union(const RawNode &that)
    {
      bool changed = false;

      changed |= HasIndirectJump != that.HasIndirectJump;
      HasIndirectCall |= that.HasIndirectJump;

      changed |= HasIndirectCall != that.HasIndirectCall;
      HasIndirectCall |= that.HasIndirectCall;

      changed |= Anchor != that.Anchor;
      Anchor |= that.Anchor;

      for (Func *f : that.Entries) {
        changed |= Entries.insert(f).second;
      }
      for (Func *f : that.Funcs) {
        changed |= Funcs.insert(f).second;
      }
      for (Object *f : that.Objects) {
        changed |= Objects.insert(f).second;
      }
      for (Block *f : that.Blocks) {
        changed |= Blocks.insert(f).second;
      }

      return changed;
    }
  };

  /// Mapping from IDs to nodes.
  UnionFind<RawNode> nodes_;
};
