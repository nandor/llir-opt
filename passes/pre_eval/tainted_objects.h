// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <array>
#include <set>
#include <unordered_map>
#include <llvm/Support/raw_ostream.h>
#include "core/adt/bitset.h"
#include "core/adt/union_find.h"
#include "core/adt/hash.h"
#include "core/adt/id.h"
#include "core/adt/queue.h"

class Func;
class Block;
class Object;
class Inst;
class BlockBuilder;



/**
 * Block-level tainted atom analysis.
 */
class TaintedObjects final {
public:
  struct Tainted {
  public:
    /// Merges all elements from the other set into this.
    bool Union(const Tainted &that);

    /// Add an element to the set.
    bool Add(ID<Object> atom);
    /// Add a function to the set.
    bool Add(ID<Func> func);
    /// Add a block to the set.
    bool Add(ID<Block> block);

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

  private:
    /// Set of individual tainted atoms.
    BitSet<Object> objects_;
    /// Set of tainted functions.
    BitSet<Func> funcs_;
    /// Set of tainted blocks.
    BitSet<Block> blocks_;
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
  std::optional<Tainted> operator[](Block &block) const;

  /**
   * Maps an object ID to an object.
   */
  Object *Map(ID<Object> id) { return objectMap_.Map(id); }

private:
  friend class BlockBuilder;

  /// Call string for context sensitivity.
  template<unsigned N>
  class CallString {
  private:
    using T = std::array<const Inst *, N>;

  public:
    CallString() : n_(0) {}

    bool operator == (const CallString<N> &that) const
    {
      return n_ == that.n_ && calls_ == that.calls_;
    }

    CallString Append(const Inst *inst) const
    {
      assert(n_ <= N && "invalid call string");
      CallString res;
      if (n_ + 1 <= N) {
        for (unsigned i = 0; i < n_; ++i) {
          res.calls_[i] = calls_[i];
        }
        res.calls_[n_] = inst;
        res.n_ = n_ + 1;
      } else {
        for (unsigned i = 1; i < n_; ++i) {
          res.calls_[i - 1] = calls_[i];
        }
        res.calls_[N - 1] = inst;
        res.n_ = N;
      }
      return res;
    }

    typename T::const_iterator begin() const { return calls_.begin(); }
    typename T::const_iterator end() const { return calls_.end(); }

  private:
    T calls_;
    unsigned n_;
  };

  /// Degree of context-sensitivity.
  static constexpr unsigned N = 1;

  /// Information extracted from the program.
  ///
  /// These blocks split the original ones at call sites.
  struct BlockInfo {
    /// Taint from instructions in the block.
    Tainted Taint;
    /// Successor blocks.
    BitSet<BlockInfo> Successors;

    void Union(const BlockInfo &that)
    {
      llvm_unreachable("not implemented");
    }
  };

  /// Explores a function.
  ID<BlockInfo> Explore(const CallString<N> &cs, Func &func);
  /// Explores a block.
  ID<BlockInfo> Explore(const CallString<N> &cs, Block &block);
  /// Propagates information in the graph.
  void Propagate();
  /// Explores indirect call and jump sites.
  bool ExpandIndirect();

  /// Hash for call string +
  template<typename T>
  struct KeyHash {
    std::size_t operator()(const std::pair<CallString<N>, T> &key) const
    {
      std::size_t hash = 0;
      for (const Inst *inst : key.first) {
        hash_combine(hash, std::hash<const Inst *>{}(inst));
      }
      hash_combine(hash, std::hash<T>{}(key.second));
      return hash;
    }
  };

  /// Mapping from instructions to blocks.
  std::unordered_map<
    std::pair<CallString<N>, const Inst *>,
    ID<BlockInfo>,
    KeyHash<const Inst *>
  > instToBlock_;
  /// Exit nodes of functions.
  std::unordered_map<
    std::pair<CallString<N>, const Func *>,
    ID<BlockInfo>,
    KeyHash<const Func *>
  > exitToBlock_;
  /// Union-Find data structure mapping block IDs to blocks.
  UnionFind<BlockInfo> blocks_;

  /// Description of an indirect call site.
  struct IndirectCall {
    CallString<N> CS;
    ID<BlockInfo> From;
    ID<BlockInfo> Cont;

    IndirectCall(const CallString<N> &cs, ID<BlockInfo> from, ID<BlockInfo> cont)
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
    CallString<N> CS;
    ID<BlockInfo> From;

    IndirectJump(const CallString<N> &cs, ID<BlockInfo> from)
      : CS(cs)
      , From(from)
    {
    }
  };
  /// List of indirect jumps.
  std::vector<IndirectJump> indirectJumps_;

  /// Aggregation of all blocks at all call sites.
  std::unordered_map<Inst *, BitSet<BlockInfo>> blockSites_;

  /// Mapping from objects to IDs.
  template<typename T>
  class ObjectToID {
  public:
    ID<T> Map(T *t)
    {
      if (auto it = objToID_.find(t); it != objToID_.end()) {
        return it->second;
      }
      auto id = ID<T>(idToObj_.size());
      idToObj_.push_back(t);
      objToID_.emplace(t, id);
      return id;
    }

    T *Map(ID<T> id) { return idToObj_[id]; }

  private:
    /// Mapping from pointers to IDs.
    std::unordered_map<T *, ID<T>> objToID_;
    /// Mapping from IDs to pointers.
    std::vector<T *> idToObj_;
  };

  /// Mapping between blocks and IDs.
  ObjectToID<Block> blockMap_;
  /// Mapping between functions and IDs.
  ObjectToID<Func> funcMap_;
  /// Mapping between objects and IDs.
  ObjectToID<Object> objectMap_;

  /// Queue of blocks to explore.
  Queue<BlockInfo> queue_;

  /// Maps an instruction to a block.
  ID<BlockInfo> MapInst(const CallString<N> &cs, Inst *inst);
  /// Maps a function to its exit block.
  ID<BlockInfo> MapFunc(const CallString<N> &cs, Func *func);
  /// Maps a block to a block info object ID.
  ID<BlockInfo> MapBlock(const CallString<N> &cs, Block *block);

  /// Creates an indirect landing site.
  ID<BlockInfo> CreateBlock();
};
