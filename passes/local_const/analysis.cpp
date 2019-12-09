// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/local_const/analysis.h"
#include "passes/local_const/context.h"



// -----------------------------------------------------------------------------
Analysis::Analysis(Func &func, LCContext &context)
  : func_(func)
  , context_(context)
{
  // Generate unique block IDs.
  for (Block &block : func) {
    blockToIndex_.insert({ &block, blocks_.size() });
    blocks_.push_back(&block);
  }

  // Build kill/gen for individual blocks.
  for (Block &block : func) {
    BlockInfo &blockInfo = blocks_[blockToIndex_[&block]];

    // Construct fast pred/succ information.
    for (const Block *pred : block.predecessors()) {
      blockInfo.Preds.push_back(blockToIndex_[pred]);
    }
    for (const Block *succ : block.successors()) {
      blockInfo.Succs.push_back(blockToIndex_[succ]);
    }
  }
}

// -----------------------------------------------------------------------------
void Analysis::BuildCall(Inst *I)
{
  auto &kg = GetInfo(I);
  BuildRoots(I, kg);
  BuildExtern(I, kg);
  BuildReturn(I, kg);
}

// -----------------------------------------------------------------------------
void Analysis::BuildLongJmp(Inst *I)
{
  auto &kg = GetInfo(I);
  BuildExtern(I, kg);
  for (auto &obj : func_.objects()) {
    kg.GenLiveAlloc.Insert(context_.Frame(obj.Index)->GetID());
  }
}

// -----------------------------------------------------------------------------
void Analysis::BuildAlloc(Inst *I)
{
  BuildRoots(I, GetInfo(I));
}

// -----------------------------------------------------------------------------
void Analysis::BuildStore(StoreInst *I, LCSet *addr)
{
  auto &kg = GetInfo(I);

  std::optional<Element> elem;
  addr->points_to_elem([&elem, &kg, I](LCAlloc *alloc, LCIndex idx) {
    auto allocID = alloc->GetID();
    if (!kg.KillReachElem.empty()) {
      for (size_t i = 0; i < I->GetStoreSize(); ++i) {
        kg.KillReachElem.insert({ alloc->GetID(), idx + i });
      }
    } else if (elem) {
      elem = std::nullopt;
      for (size_t i = 0; i < I->GetStoreSize(); ++i) {
        kg.KillReachElem.insert({ elem->first, elem->second + i });
        kg.KillReachElem.insert({ alloc->GetID(), idx + i });
      }
    } else {
      elem = { allocID, idx };
    }
    kg.KillLiveElem.insert({ allocID, idx });
  });
  addr->points_to_range([&elem, &kg](LCAlloc *alloc) {
    elem = std::nullopt;
    kg.KillReachAlloc.Insert(alloc->GetID());
  });

  if (elem) {
    kg.GenReachElem = { *elem, I };
  }
}

// -----------------------------------------------------------------------------
void Analysis::BuildClobber(Inst *I, LCSet *addr)
{
  auto &kg = GetInfo(I);
  addr->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.KillReachAlloc.Insert(allocID);
    kg.GenLiveAlloc.Insert(allocID);
  });
  addr->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    Element elem{ alloc->GetID(), index };
    kg.KillReachElem.insert(elem);
    kg.GenLiveElem.insert(elem);
    kg.KillLiveElem.insert(elem);
  });
}

// -----------------------------------------------------------------------------
void Analysis::BuildGen(Inst *I, LCSet *addr)
{
  auto &kg = GetInfo(I);
  addr->points_to_range([&kg](LCAlloc *alloc) {
    kg.GenLiveAlloc.Insert(alloc->GetID());
  });
  addr->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    kg.GenLiveElem.insert({ alloc->GetID(), index });
  });
}

// -----------------------------------------------------------------------------
void Analysis::Solve()
{
  for (BlockInfo &blockInfo : blocks_) {
    // Forward analysis: construct kill-gen for the block.
    for (auto it = blockInfo.I.begin(); it != blockInfo.I.end(); ++it) {
      const auto &kg = *it;

      blockInfo.KillReachAlloc.Union(kg.KillReachAlloc);
      for (auto &elem : kg.KillReachElem) {
        blockInfo.KillReachElem.insert(elem);
      }

      ElementSet newSet;
      for (auto &elem : blockInfo.GenReachElem) {
        if (blockInfo.KillReachElem.count(elem.first)) {
          continue;
        }
        if (blockInfo.KillReachAlloc.Contains(elem.first.first)) {
          continue;
        }
        newSet.insert(elem);
      }

      if (kg.GenReachElem) {
        newSet.insert(*kg.GenReachElem);
      }

      blockInfo.GenReachElem = newSet;
    }

    // Backward analysis: construct kill-gen for the block.
    for (auto it = blockInfo.I.rbegin(); it != blockInfo.I.rend(); ++it) {
      const auto &kg = *it;;

      // gen' = gen - killNew U genNew
      blockInfo.GenLiveAlloc.Union(kg.GenLiveAlloc);
      for (auto elem : kg.KillLiveElem) {
        blockInfo.GenLiveElem.erase(elem);
      }
      for (auto elem : kg.GenLiveElem) {
        blockInfo.GenLiveElem.insert(elem);
      }

      // kill' = kill U killNew
      for (auto elem : kg.KillLiveElem) {
        blockInfo.KillLiveElem.insert(elem);
      }
    }
  }

  // Solve iteratively.
  {
    bool changed;

    // Iterate forward, computing reaching defs.
    do {
      changed = false;
      for (auto it = blocks_.begin(); it != blocks_.end(); ++it) {
        BlockInfo &info = *it;

        ElementSet reach;
        for (unsigned prev : info.Preds) {
          for (const auto &elem : blocks_[prev].ReachOut) {
            if (info.KillReachElem.count(elem.first)) {
              continue;
            }
            if (info.KillReachAlloc.Contains(elem.first.first)) {
              continue;
            }
            reach.insert(elem);
          }
        }

        for (auto &elem : info.GenReachElem) {
          reach.insert(elem);
        }

        changed = reach != info.ReachOut;
        info.ReachOut = reach;
      }
    } while (changed);

    // Iterate backward, computing live variables.
    do {
      changed = false;
      for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it) {
        BlockInfo &info = *it;

        // Compute the live-out set from successors.
        BitSet<LCAlloc> allocs;
        std::set<Element> elemsOut;
        for (unsigned succ : info.Succs) {
          BlockInfo &succInfo = blocks_[succ];
          allocs.Union(succInfo.LiveAllocsIn);
          for (auto &elem : succInfo.LiveElemsIn) {
            elemsOut.insert(elem);
          }
        }

        // live-in = gen U (live-out - kill)
        allocs.Union(info.GenLiveAlloc);

        std::set<Element> elems;
        for (auto &elem : elemsOut) {
          if (info.KillLiveElem.count(elem) != 0) {
            continue;
          }
          elems.insert(elem);
        }
        for (auto &elem : info.GenLiveElem) {
          elems.insert(elem);
        }

        changed = allocs != info.LiveAllocsIn || elems != info.LiveElemsIn;
        info.LiveAllocsIn = allocs;
        info.LiveElemsIn = elems;
      }
    } while (changed);
  }
}

// -----------------------------------------------------------------------------
void Analysis::ReachingDefs(std::function<void(Inst *, const ReachSet &)> && f)
{
  for (Block &block : func_) {
    BlockInfo &blockInfo = blocks_[blockToIndex_[&block]];

    // Construct the reach-in set from reach-outs.
    ElementSet reachIn;
    for (const auto prev : blockInfo.Preds) {
      for (const auto &elem : blocks_[prev].ReachOut) {
        reachIn.insert(elem);
      }
    }

    std::map<Element, Inst *> defs;
    for (auto &kg : blockInfo.I) {
      // Compute the reaching def set.
      for (auto it = defs.begin(); it != defs.end(); ) {
        if (kg.KillReachElem.count(it->first)) {
          defs.erase(it++);
          continue;
        }
        if (kg.KillReachAlloc.Contains(it->first.first)) {
          defs.erase(it++);
          continue;
        }
        ++it;
      }
      if (kg.GenReachElem) {
        auto it = defs.find(kg.GenReachElem->first);
        if (it == defs.end()) {
          defs.insert({ kg.GenReachElem->first, kg.GenReachElem->second });
        } else {
          it->second = kg.GenReachElem->second;
        }
      }

      // Simplify loads, if possible.
      f(kg.I, ReachSet(defs));
    }
  }
}

// -----------------------------------------------------------------------------
void Analysis::LiveStores(std::function<void(Inst *, const LiveSet &)> && f)
{
  // Compute live-in/outs and remove dead stores.
  for (Block &block : func_) {
    BlockInfo &blockInfo = blocks_[blockToIndex_[&block]];

    // Compute the live-out set from live-ins of successors.
    BitSet<LCAlloc> allocs;
    std::set<Element> elems;
    for (const auto succ : blockInfo.Succs) {
      const auto &succInfo = blocks_[succ];
      allocs.Union(succInfo.LiveAllocsIn);
      for (const auto &elem : blocks_[succ].LiveElemsIn) {
        elems.insert(elem);
      }
    }

    for (auto it = blockInfo.I.rbegin(); it != blockInfo.I.rend(); ++it) {
      const auto &kg = *it;
      // This is the live-out set - invoke the callback.
      f(kg.I, LiveSet(allocs, elems));

      // Compute the live range set.
      allocs.Union(kg.GenLiveAlloc);

      // Compute the live elem set.
      {
        std::set<Element> newElems = kg.GenLiveElem;
        for (auto &elem : elems) {
          if (kg.KillLiveElem.count(elem) != 0) {
            continue;
          }
          newElems.insert(elem);
          allocs.Insert(elem.first);
        }
        elems = newElems;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void Analysis::BuildExtern(Inst *I, KillGen &kg)
{
  LCSet *ext = context_.Extern();
  ext->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.KillReachAlloc.Insert(allocID);
    kg.GenLiveAlloc.Insert(allocID);
  });
  ext->points_to_elem([&kg](LCAlloc *alloc, LCIndex index) {
    auto allocID = alloc->GetID();
    kg.KillReachElem.insert({ allocID, index });
    kg.GenLiveElem.insert({ allocID, index });
  });
}

// -----------------------------------------------------------------------------
void Analysis::BuildRoots(Inst *I, KillGen &kg)
{
  LCSet *root = context_.Root();
  root->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.KillReachAlloc.Insert(allocID);
    kg.GenLiveAlloc.Insert(allocID);
  });
  root->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    Element elem{ alloc->GetID(), index };
    kg.KillReachElem.insert(elem);
    kg.KillLiveElem.insert(elem);
    kg.GenLiveElem.insert(elem);
  });

  if (auto *live = context_.GetLive(I)) {
    live->points_to_range([&kg](LCAlloc *alloc) {
      auto allocID = alloc->GetID();
      kg.KillReachAlloc.Insert(allocID);
      kg.GenLiveAlloc.Insert(allocID);
    });
  }
}

// -----------------------------------------------------------------------------
void Analysis::BuildReturn(Inst *I, KillGen &kg)
{
  if (LCSet *ret = context_.GetNode(I)) {
    ret->points_to_range([&kg](LCAlloc *alloc) {
      kg.GenLiveAlloc.Insert(alloc->GetID());
    });
    ret->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
      kg.GenLiveElem.insert({ alloc->GetID(), index });
    });
  }
}
