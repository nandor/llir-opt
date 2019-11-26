// This file if part of the genm-opt project.
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
public:
  Analysis(Func &func, LCContext &context);

  void BuildCall(Inst *I);
  void BuildAlloc(Inst *I);
  void BuildStore(StoreInst *I, LCSet *addr);
  void BuildClobber(Inst *I, LCSet *addr);
  void BuildGen(Inst *I, LCSet *addr);

  void Solve();

  /// Traverses the reaching defs results.
  class ReachSet {
  public:
    ReachSet(const std::map<Element, Inst *> &defs) : defs_(defs) {}

    StoreInst *Find(const Element &elem) const
    {
      if (auto it = defs_.find(elem); it != defs_.end()) {
        return ::dyn_cast_or_null<StoreInst>(it->second);
      }
      return nullptr;
    }

  private:
    std::map<Element, Inst *> defs_;
  };
  void ReachingDefs(std::function<void(Inst *, const ReachSet &)> && f);

  /// Traverses the LVA results.
  class LiveSet {
  public:
    LiveSet(BitSet<LCAlloc> &allocs, std::set<Element> &elems)
      : allocs_(allocs)
      , elems_(elems)
    {
    }

    bool Contains(ID<LCAlloc> alloc, uint64_t index) const
    {
      return elems_.find({ alloc, index }) != elems_.end();
    }

    bool Contains(ID<LCAlloc> alloc) const
    {
      return allocs_.Contains(alloc);
    }

  private:
    BitSet<LCAlloc> &allocs_;
    std::set<Element> &elems_;
  };
  void LiveStores(std::function<void(Inst *, const LiveSet &)> && f);

private:
  /// Set of indices into objects mapping to values.
  typedef std::set<std::pair<Element, Inst *>> ElementSet;

  struct KillGen {
    /// Instruction which generates or clobbers.
    Inst *I;

    /// Element generated - location and value.
    std::optional<std::pair<Element, Inst *>> GenReachElem;
    /// Kill of all elements of this allocation.
    BitSet<LCAlloc> KillReachAlloc;
    /// Kill individual elements in the allocation.
    std::set<Element> KillReachElem;

    /// Gen of whole allocations.
    BitSet<LCAlloc> GenLiveAlloc;
    /// Kill of whole allocations.
    BitSet<LCAlloc> KillLiveAlloc;
    /// Gen of individual elements.
    std::set<Element> GenLiveElem;
    /// Kill of individual elements.
    std::set<Element> KillLiveElem;

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

    /// Gens of the block.
    ElementSet GenReachElem;
    /// Kill of all elements of this allocation.
    BitSet<LCAlloc> KillReachAlloc;
    /// Kill individual elements in the allocation.
    std::set<Element> KillReachElem;

    /// Reaching defs.
    ElementSet ReachOut;

    /// Gen of whole allocations.
    BitSet<LCAlloc> GenLiveAlloc;
    /// Gen of individual elements.
    std::set<Element> GenLiveElem;
    /// Kill of individual elements.
    std::set<Element> KillLiveElem;

    /// Live elements.
    BitSet<LCAlloc> LiveAllocsIn;
    /// Live allocations.
    std::set<Element> LiveElemsIn;

    BlockInfo(Block *B) : B(B) {}
  };

private:
  /// Returns a kill-gen set for an instruction.
  KillGen &GetInfo(Inst *I)
  {
    return blocks_[blockToIndex_[I->getParent()]].I.emplace_back(I);
  }

  /// Handles clobbers/defs for a function call.
  void BuildRoots(Inst *I, KillGen &kg);

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
