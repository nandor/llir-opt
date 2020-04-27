// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include "core/inst.h"
#include "core/insts.h"
#include "core/cast.h"
#include "core/func.h"
#include "passes/local_const/graph.h"


/// Forward declarations.
class LCContext;

/// Type to identify an element of an allocation.
typedef std::pair<ID<LCAlloc>, uint64_t> Element;



/**
 * Wrapper for data flow analyses relying on pointers.
 */
class Analysis {
private:
  struct LiveKillGen;
  struct ReachabilityGen;
  struct ReachabilityKill;

public:
  Analysis(Func &func, LCContext &context);

  void BuildCall(Inst *I);
  void BuildLongJmp(Inst *I);
  void BuildAlloc(Inst *I);
  void BuildStore(StoreInst *I, LCSet *addr);
  void BuildClobber(Inst *I, LCSet *addr);
  void BuildGen(Inst *I, LCSet *addr);

  void Solve();

  /// Traverses the reaching defs results.
  class ReachSet {
  public:
    ReachSet() {}

    StoreInst *Find(const Element &elem) const
    {
      if (auto it = defs_.find(elem); it != defs_.end()) {
        return ::dyn_cast_or_null<StoreInst>(it->second);
      }
      return nullptr;
    }

    void Minus(const ReachabilityKill &that);

    void Union(const ReachabilityGen &that);

    void Union(const ReachSet &that);

    bool operator==(const ReachSet &that) const { return defs_ == that.defs_; }

  private:
    friend class Analysis;
    std::map<Element, Inst *> defs_;
  };
  void ReachingDefs(std::function<void(Inst *, const ReachSet &)> && f);

  /// Traverses the LVA results.
  class LiveSet {
  public:
    LiveSet() {}

    bool Contains(ID<LCAlloc> alloc, uint64_t index) const
    {
      return elems_.find({ alloc, index }) != elems_.end();
    }

    bool Contains(ID<LCAlloc> alloc) const
    {
      return allocs_.Contains(alloc);
    }

    void Minus(const LiveKillGen &that);

    void Union(const LiveKillGen &that);

    void Union(const LiveSet &that);

    bool operator==(const LiveSet &that) const
    {
      return allocs_ == that.allocs_ && elems_ == that.elems_;
    }

  private:
    friend class Analysis;
    BitSet<LCAlloc> allocs_;
    std::set<Element> elems_;
  };
  void LiveStores(std::function<void(Inst *, const LiveSet &)> && f);

private:
  /// Gen and Kill info for live variables.
  struct LiveKillGen {
    BitSet<LCAlloc> Allocs;
    std::set<Element> Elems;

    // Set difference: this - that.
    void Minus(const LiveKillGen &that);

    // Set union: this U that.
    void Union(const LiveKillGen &that);
  };

  struct ReachabilityGen {
    void Minus(const ReachabilityKill &that);
    void Union(const ReachabilityGen &that);

    /// Gens of the block.
    std::map<Element, Inst *> Elems;
  };

  struct ReachabilityKill {
    /// Kill of all elements of this allocation.
    BitSet<LCAlloc> Allocs;
    /// Kill individual elements in the allocation.
    std::set<Element> Elems;

    void Union(const ReachabilityKill &that);
  };

  struct KillGen {
    /// Instruction which generates or clobbers.
    Inst *I;

    /// Liveness gen.
    LiveKillGen LiveGen;
    /// Liveness kill.
    LiveKillGen LiveKill;

    /// Reachability gen.
    ReachabilityGen ReachGen;
    /// Reachability kill.
    ReachabilityKill ReachKill;

    KillGen(Inst *I) : I(I) {}
  };

  struct BlockInfo {
    /// Block for which the info is generated.
    Block *B;
    /// Predecessor indices.
    llvm::SmallVector<uint64_t, 5> Preds;
    /// Successor indices.
    llvm::SmallVector<uint64_t, 5> Succs;
    /// Kill-gen of individual elements.
    std::vector<KillGen> I;

    /// Gen of block.
    LiveKillGen LiveGen;
    /// Kill of block.
    LiveKillGen LiveKill;
    /// Per-block live in.
    LiveSet Live;

    /// Reachability gen.
    ReachabilityGen ReachGen;
    /// Reachability kill.
    ReachabilityKill ReachKill;
    /// Reaching defs.
    ReachSet Reach;

    BlockInfo(Block *B) : B(B) {}
  };

private:
  /// Returns a kill-gen set for an instruction.
  KillGen &GetInfo(Inst *I)
  {
    return blocks_[blockToIndex_[I->getParent()]].I.emplace_back(I);
  }

  /// Handles clobbers/defs to externs.
  void BuildExtern(Inst *I, KillGen &kg);
  /// Handles clobbers/defs for a function call.
  void BuildRoots(Inst *I, KillGen &kg);
  /// Handles defs of the extern set on return.
  void BuildReturn(Inst *I, KillGen &kg);

private:
  /// Context of the function.
  LCContext &context_;
  /// Reference to the function.
  Func &func_;
  /// Block information for data flow analyses.
  std::vector<BlockInfo> blocks_;
  /// Mapping from blocks_ to indices.
  std::unordered_map<const Block *, uint64_t> blockToIndex_;
  /// Mapping from instructions to nodes.
  std::unordered_map<const Inst *, ID<LCSet>> nodes_;
};
