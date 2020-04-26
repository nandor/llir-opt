// This file if part of the llir-opt project.
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
    kg.LiveGen.Allocs.Insert(context_.Frame(obj.Index)->GetID());
  }
}

// -----------------------------------------------------------------------------
void Analysis::BuildAlloc(Inst *I)
{
  BuildRoots(I, GetInfo(I));
}

// -----------------------------------------------------------------------------
void Analysis::BuildStore(StoreInst *st, LCSet *addr)
{
  const Type ty = st->GetVal()->GetType(0);
  auto &kg = GetInfo(st);

  std::optional<Element> elem;
  addr->points_to_elem([&elem, &kg, st, ty](LCAlloc *alloc, LCIndex idx) {
    auto allocID = alloc->GetID();
    if (!kg.KillReachElem.empty()) {
      for (size_t i = 0, n = GetSize(ty); i < n; ++i) {
        kg.KillReachElem.insert({ alloc->GetID(), idx + i });
      }
    } else if (elem) {
      elem = std::nullopt;
      for (size_t i = 0, n = GetSize(ty); i < n; ++i) {
        kg.KillReachElem.insert({ elem->first, elem->second + i });
        kg.KillReachElem.insert({ alloc->GetID(), idx + i });
      }
    } else {
      elem = { allocID, idx };
    }
    kg.LiveKill.Elems.insert({ allocID, idx });
  });
  addr->points_to_range([&elem, &kg](LCAlloc *alloc) {
    elem = std::nullopt;
    kg.KillReachAlloc.Insert(alloc->GetID());
  });

  if (elem) {
    kg.GenReachElem = { *elem, st };
  }
}

// -----------------------------------------------------------------------------
void Analysis::BuildClobber(Inst *I, LCSet *addr)
{
  auto &kg = GetInfo(I);
  addr->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.KillReachAlloc.Insert(allocID);
    kg.LiveGen.Allocs.Insert(allocID);
  });
  addr->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    Element elem{ alloc->GetID(), index };
    kg.KillReachElem.insert(elem);
    kg.LiveGen.Elems.insert(elem);
    kg.LiveKill.Elems.insert(elem);
  });
}

// -----------------------------------------------------------------------------
void Analysis::BuildGen(Inst *I, LCSet *addr)
{
  auto &kg = GetInfo(I);
  addr->points_to_range([&kg](LCAlloc *alloc) {
    kg.LiveGen.Allocs.Insert(alloc->GetID());
  });
  addr->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    kg.LiveGen.Elems.insert({ alloc->GetID(), index });
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
      const auto &kg = *it;

      // gen' = gen - killNew U genNew
      blockInfo.LiveGen.Minus(kg.LiveKill);
      blockInfo.LiveGen.Union(kg.LiveGen);

      // kill' = kill U killNew
      blockInfo.LiveKill.Union(kg.LiveKill);
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
        AllocSet allocs;
        for (unsigned succ : it->Succs) {
          allocs.Union(blocks_[succ].Live);
        }

        // live-in = gen U (live-out - kill)
        allocs.Union(it->LiveGen);
        allocs.Minus(it->LiveKill);

        changed = !(it->Live == allocs);
        it->Live = allocs;
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
    AllocSet set;
    for (const auto succ : blockInfo.Succs) {
      set.Union(blocks_[succ].Live);
    }

    for (auto it = blockInfo.I.rbegin(); it != blockInfo.I.rend(); ++it) {
      // This is the live-out set - invoke the callback.
      f(it->I, LiveSet(set.Allocs, set.Elems));

      // Compute the live range set.
      set.Union(it->LiveGen);
      set.Minus(it->LiveKill);
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
    kg.LiveGen.Allocs.Insert(allocID);
  });
  ext->points_to_elem([&kg](LCAlloc *alloc, LCIndex index) {
    auto allocID = alloc->GetID();
    kg.KillReachElem.insert({ allocID, index });
    kg.LiveGen.Elems.insert({ allocID, index });
  });
}

// -----------------------------------------------------------------------------
void Analysis::BuildRoots(Inst *I, KillGen &kg)
{
  LCSet *root = context_.Root();
  root->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.KillReachAlloc.Insert(allocID);
    kg.LiveGen.Allocs.Insert(allocID);
  });
  root->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    Element elem{ alloc->GetID(), index };
    kg.KillReachElem.insert(elem);
    kg.LiveKill.Elems.insert(elem);
    kg.LiveGen.Elems.insert(elem);
  });

  if (auto *live = context_.GetLive(I)) {
    live->points_to_range([&kg](LCAlloc *alloc) {
      auto allocID = alloc->GetID();
      kg.KillReachAlloc.Insert(allocID);
      kg.LiveGen.Allocs.Insert(allocID);
    });
  }
}

// -----------------------------------------------------------------------------
void Analysis::BuildReturn(Inst *I, KillGen &kg)
{
  if (LCSet *ret = context_.GetNode(I)) {
    ret->points_to_range([&kg](LCAlloc *alloc) {
      kg.LiveGen.Allocs.Insert(alloc->GetID());
    });
    ret->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
      kg.LiveGen.Elems.insert({ alloc->GetID(), index });
    });
  }
}
