// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <array>
#include <set>
#include <unordered_map>
#include <queue>

#include "core/adt/bitset.h"
#include "core/adt/union_find.h"
#include "core/adt/hash.h"
#include "core/adt/id.h"
#include "passes/pre_eval/flow_graph.h"

class Func;
class Block;
class Object;
class Inst;
class BlockBuilder;
class BlockInfoNode;



/**
 * Block-level tainted atom analysis.
 */
class TaintedObjects final {
public:
  struct Tainted {
  public:
    /// Creates an empty taint node.
    Tainted() {}

    /// Creates a taint set from a flow graph node.
    Tainted(const FlowGraph::Node *node)
      : objects_(node->Objects)
      , blocks_(node->Blocks)
      , funcs_(node->Funcs)
    {
    }

    /// Merges all elements from the other set into this.
    bool Union(const Tainted &that);

    // Iterator over functions.
    BitSet<Func>::iterator funcs_begin() { return funcs_.begin(); }
    BitSet<Func>::iterator funcs_end() { return funcs_.end(); }
    llvm::iterator_range<BitSet<Func>::iterator> funcs()
    {
      return llvm::make_range(funcs_begin(), funcs_end());
    }

    // Iterator over blocks.
    BitSet<Block>::iterator blocks_begin() { return blocks_.begin(); }
    BitSet<Block>::iterator blocks_end() { return blocks_.end(); }
    llvm::iterator_range<BitSet<Block>::iterator> blocks()
    {
      return llvm::make_range(blocks_begin(), blocks_end());
    }

    // Iterator over objects.
    BitSet<Object>::iterator objects_begin() { return objects_.begin(); }
    BitSet<Object>::iterator objects_end() { return objects_.end(); }
    llvm::iterator_range<BitSet<Object>::iterator> objects()
    {
      return llvm::make_range(objects_begin(), objects_end());
    }

    /// Checks if the object is empty.
    bool Empty() const
    {
      return objects_.Empty() && funcs_.Empty() && blocks_.Empty();
    }

  private:
    /// Set of individual tainted atoms.
    BitSet<Object> objects_;
    /// Set of tainted functions.
    BitSet<Func> funcs_;
    /// Set of tainted blocks.
    BitSet<Block> blocks_;
  };

  /// Information extracted from the program.
  ///
  /// These blocks split the original ones at call sites.
  struct BlockInfo {
    /// ID of the object.
    ID<BlockInfo> BlockID;
    /// Parent object.
    TaintedObjects *Objects;
    /// Taint from instructions in the block.
    Tainted Taint;
    /// Successor blocks.
    BitSet<BlockInfo> Successors;

    BlockInfo(
        ID<BlockInfo> blockID,
        TaintedObjects *objects,
        const FlowGraph::Node *node)
      : BlockID(blockID)
      , Objects(objects)
      , Taint(node)
    {
    }

    void Union(const BlockInfo &that)
    {
      Taint.Union(that.Taint);
      Successors.Union(that.Successors);
    }

    /// Iterator over block infos.
    class iterator {
    public:
      iterator(TaintedObjects *objects, BitSet<BlockInfo>::iterator it)
        : objects_(objects)
        , it_(it)
      {
      }

      bool operator!=(const iterator &that) const { return !(*this == that); }
      bool operator==(const iterator &that) const { return it_ == that.it_; }

      iterator &operator++()
      {
        ++it_;
        return *this;
      }

      iterator operator++(int)
      {
        iterator it(*this);
        ++*this;
        return it;
      }

      BlockInfo *operator*() const { return objects_->blocks_.Map(*it_); }

    private:
      TaintedObjects *objects_;
      BitSet<BlockInfo>::iterator it_;
    };

    iterator begin() { return iterator(Objects, Successors.begin()); }
    iterator end() { return iterator(Objects, Successors.end()); }
  };

public:
  /**
   * Runs the analysis, using func as the entry.
   *
   * @param entry The entry function of the program.
   */
  TaintedObjects(Func &entry);

  /**
   * Cleanup.
   */
  ~TaintedObjects();

  /**
   * Returns the set of tainted atoms reaching a block.
   */
  std::optional<Tainted> operator[](const Inst &inst) const;

  /**
   * Map an object ID to an object.
   */
  const Object *operator[](ID<Object> id) const { return graph_[id]; }

  /**
   * Returns the entry node.
   */
  BlockInfo *GetEntryNode() { return blocks_.Map(entry_); }

private:
  friend class BlockBuilder;
  friend class BlockInfoNode;

  /// Call string for context sensitivity.
  class CallString {
  public:
    CallString(const FlowGraph::Node *context)
      : indirect_(false)
      , context_(context)
    {
    }

    bool operator == (const CallString &that) const
    {
      return indirect_ == that.indirect_ && context_ == that.context_;
    }

    CallString Indirect() const
    {
      CallString res(*this);
      res.indirect_ = true;
      return res;
    }

    CallString Context(const FlowGraph::Node *context) const
    {
      CallString res(*this);
      if (!indirect_) {
        res.context_ = context;
      }
      return res;
    }

    size_t Hash() const
    {
      size_t hash = 0;
      hash_combine(hash, indirect_);
      hash_combine(hash, std::hash<const FlowGraph::Node *>{}(context_));
      return hash;
    }

  //private:
    bool indirect_;
    const FlowGraph::Node *context_;
  };

  /// Item in the explore queue.
  struct ExploreItem {
    CallString CS;
    const Func *F;
    ID<BlockInfo> Site;
    BitSet<BlockInfo> Cont;

    ExploreItem(
        const CallString &cs,
        const Func *f,
        ID<BlockInfo> site,
        const BitSet<BlockInfo> &cont)
      : CS(cs)
      , F(f)
      , Site(site)
      , Cont(cont)
    {
    }
  };
  /// Queue of items to explore.
  std::queue<ExploreItem> explore_;

  /// Result of exploring a function.
  struct FunctionID {
    /// Entry block of the function.
    ID<BlockInfo> Entry;
    /// Exit block from the function.
    BitSet<BlockInfo> Exit;

    FunctionID(ID<BlockInfo> entry, BitSet<BlockInfo> exit)
      : Entry(entry), Exit(exit)
    {
    }
  };

  /// Handles functions in the explore queue.
  void ExploreQueue();
  /// Explores a node.
  ID<BlockInfo> Visit(
      const CallString &cs,
      const FlowGraph::Node *node,
      BitSet<BlockInfo> *exits = nullptr
  );
  /// Explores a function.
  FunctionID &Visit(const CallString &cs, const Func &func);

  /// Explores a function.
  FunctionID &Explore(const CallString &cs, const Func &func)
  {
    auto &id = Visit(cs, func);
    ExploreQueue();
    return id;
  }

  /// Explores a block.
  ID<BlockInfo> Explore(const CallString &cs, const Block &block)
  {
    auto id = Visit(cs, graph_[&block]);
    ExploreQueue();
    return id;
  }

  /// Propagates information in the graph.
  void Propagate();
  /// Explores indirect call and jump sites.
  bool ExpandIndirect();

  /// Key to identify blocks and functions.
  template<typename T>
  struct Key {
    CallString CS;
    T K;

    bool operator==(const Key<T> &that) const
    {
      return CS == that.CS && K == that.K;
    }
  };

  /// Hash for context + call string + pointer.
  template<typename T>
  struct KeyHash {
    std::size_t operator()(const Key<T> &key) const
    {
      std::size_t hash = 0;
      hash_combine(hash, key.CS.Hash());
      hash_combine(hash, std::hash<T>{}(key.K));
      return hash;
    }
  };

  /// Mapping from blocks to block info object IDs.
  std::unordered_map<
    Key<const FlowGraph::Node *>,
    ID<BlockInfo>,
    KeyHash<const FlowGraph::Node *>
  > blockIDs_;
  /// Mapping from functions to function IDs.
  std::unordered_map<ID<BlockInfo>, FunctionID> funcIDs_;

  /// Union-Find data structure mapping block IDs to blocks.
  UnionFind<BlockInfo> blocks_;

  /// Description of an indirect call site.
  struct IndirectCall {
    CallString CS;
    ID<BlockInfo> From;
    BitSet<BlockInfo> Cont;

    IndirectCall(
        const CallString &cs,
        ID<BlockInfo> from,
        const BitSet<BlockInfo> &cont)
      : CS(cs)
      , From(from)
      , Cont(cont)
    {
    }
  };
  /// List of indirect calls.
  std::vector<IndirectCall> indirectCalls_;

  /// Description of an indirect jump site.
  struct IndirectJump {
    CallString CS;
    ID<BlockInfo> From;

    IndirectJump(const CallString &cs, ID<BlockInfo> from)
      : CS(cs)
      , From(from)
    {
    }
  };
  /// List of indirect jumps.
  std::vector<IndirectJump> indirectJumps_;

  /// Aggregation of all blocks at all call sites.
  std::unordered_map<const Inst *, BitSet<BlockInfo>> blockSites_;

  /// Underlying flow graph.
  FlowGraph graph_;
  /// Set of blocks executed once.
  std::set<const Block *> single_;
  /// ID of the entry node.
  ID<BlockInfo> entry_;
};
