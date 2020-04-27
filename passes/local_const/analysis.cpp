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
    if (!kg.ReachKill.Elems.empty()) {
      for (size_t i = 0, n = GetSize(ty); i < n; ++i) {
        kg.ReachKill.Elems.insert({ alloc->GetID(), idx + i });
      }
    } else if (elem) {
      elem = std::nullopt;
      for (size_t i = 0, n = GetSize(ty); i < n; ++i) {
        kg.ReachKill.Elems.insert({ elem->first, elem->second + i });
        kg.ReachKill.Elems.insert({ alloc->GetID(), idx + i });
      }
    } else {
      elem = { allocID, idx };
    }
    kg.LiveKill.Elems.insert({ allocID, idx });
  });
  addr->points_to_range([&elem, &kg](LCAlloc *alloc) {
    elem = std::nullopt;
    kg.ReachKill.Allocs.Insert(alloc->GetID());
  });

  if (elem) {
    kg.ReachGen.Elems.emplace(*elem, st);
  }
}

// -----------------------------------------------------------------------------
void Analysis::BuildClobber(Inst *I, LCSet *addr)
{
  auto &kg = GetInfo(I);
  addr->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.ReachKill.Allocs.Insert(allocID);
    kg.LiveGen.Allocs.Insert(allocID);
  });
  addr->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    Element elem{ alloc->GetID(), index };
    kg.ReachKill.Elems.insert(elem);
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
void Analysis::BuildExtern(Inst *I, KillGen &kg)
{
  LCSet *ext = context_.Extern();
  ext->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.ReachKill.Allocs.Insert(allocID);
    kg.LiveGen.Allocs.Insert(allocID);
  });
  ext->points_to_elem([&kg](LCAlloc *alloc, LCIndex index) {
    auto allocID = alloc->GetID();
    kg.ReachKill.Elems.insert({ allocID, index });
    kg.LiveGen.Elems.insert({ allocID, index });
  });
}

// -----------------------------------------------------------------------------
void Analysis::BuildRoots(Inst *I, KillGen &kg)
{
  LCSet *root = context_.Root();
  root->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.ReachKill.Allocs.Insert(allocID);
    kg.LiveGen.Allocs.Insert(allocID);
  });
  root->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    Element elem{ alloc->GetID(), index };
    kg.ReachKill.Elems.insert(elem);
    kg.LiveKill.Elems.insert(elem);
    kg.LiveGen.Elems.insert(elem);
  });

  if (auto *live = context_.GetLive(I)) {
    live->points_to_range([&kg](LCAlloc *alloc) {
      auto allocID = alloc->GetID();
      kg.ReachKill.Allocs.Insert(allocID);
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

// -----------------------------------------------------------------------------
void Analysis::Solve()
{
  for (BlockInfo &blockInfo : blocks_) {
    // Forward analysis: construct kill-gen for the block.
    for (auto it = blockInfo.I.begin(); it != blockInfo.I.end(); ++it) {
      // kill' = kill U killNew
      blockInfo.ReachKill.Union(it->ReachKill);

      // gen' = (gen - killNew) U genNew
      blockInfo.ReachGen.Minus(it->ReachKill);
      blockInfo.ReachGen.Union(it->ReachGen);
    }

    // Backward analysis: construct kill-gen for the block.
    for (auto it = blockInfo.I.rbegin(); it != blockInfo.I.rend(); ++it) {
      // gen' = (gen - killNew) U genNew
      blockInfo.LiveGen.Minus(it->LiveKill);
      blockInfo.LiveGen.Union(it->LiveGen);

      // kill' = kill U killNew
      blockInfo.LiveKill.Union(it->LiveKill);
    }
  }

  // Solve iteratively.
  {
    bool changed;

    // Iterate forward, computing reaching defs.
    do {
      changed = false;
      for (auto it = blocks_.begin(); it != blocks_.end(); ++it) {
        ReachSet reaches;
        for (unsigned prev : it->Preds) {
          reaches.Union(blocks_[prev].Reach);
        }

        reaches.Minus(it->ReachKill);
        reaches.Union(it->ReachGen);

        changed = !(it->Reach == reaches);
        it->Reach = reaches;
      }
    } while (changed);

    // Iterate backward, computing live variables.
    do {
      changed = false;
      for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it) {
        BlockInfo &info = *it;

        // Compute the live-out set from successors.
        LiveSet allocs;
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
    ReachSet set;
    for (const auto prev : blockInfo.Preds) {
      set.Union(blocks_[prev].Reach);
    }

    for (auto it = blockInfo.I.begin(); it != blockInfo.I.end(); ++it) {
      // Compute the reaching def set.
      set.Minus(it->ReachKill);
      set.Union(it->ReachGen);

      // Simplify loads, if possible.
      f(it->I, set);
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
    LiveSet set;
    for (const auto succ : blockInfo.Succs) {
      set.Union(blocks_[succ].Live);
    }

    for (auto it = blockInfo.I.rbegin(); it != blockInfo.I.rend(); ++it) {
      // This is the live-out set - invoke the callback.
      f(it->I, set);

      // Compute the live range set.
      set.Union(it->LiveGen);
      set.Minus(it->LiveKill);
    }
  }
}

// -----------------------------------------------------------------------------
void Analysis::LiveKillGen::Minus(const LiveKillGen &that)
{
  for (auto elem : that.Elems) {
    Elems.erase(elem);
  }
}

// -----------------------------------------------------------------------------
void Analysis::LiveKillGen::Union(const LiveKillGen &that)
{
  for (auto elem : that.Elems) {
    Elems.insert(elem);
    Allocs.Insert(elem.first);
  }
  Allocs.Union(that.Allocs);
}

// -----------------------------------------------------------------------------
void Analysis::ReachSet::Minus(const ReachabilityKill &kill)
{
  for (auto it = defs_.begin(); it != defs_.end(); ) {
    if (kill.Elems.count(it->first)) {
      it = defs_.erase(it);
      continue;
    }
    if (kill.Allocs.Contains(it->first.first)) {
      it = defs_.erase(it);
      continue;
    }
    ++it;
  }
}

// -----------------------------------------------------------------------------
void Analysis::ReachSet::Union(const ReachabilityGen &gen)
{
  for (auto &elem : gen.Elems) {
    auto it = defs_.insert(elem);
    if (!it.second) {
      it.first->second = elem.second;
    }
  }
}

// -----------------------------------------------------------------------------
void Analysis::ReachSet::Union(const ReachSet &that)
{
  for (auto it = defs_.begin(); it != defs_.end(); ) {
    auto jt = that.defs_.find(it->first);
    if (jt == that.defs_.end()) {
      it = defs_.erase(it);
      continue;
    }
    if (it->second != jt->second) {
      it->second = nullptr;
    }
  }
}

// -----------------------------------------------------------------------------
void Analysis::LiveSet::Minus(const LiveKillGen &kill)
{
  for (auto elem : kill.Elems) {
    elems_.erase(elem);
  }
}

// -----------------------------------------------------------------------------
void Analysis::LiveSet::Union(const LiveKillGen &gen)
{
  for (auto elem : gen.Elems) {
    elems_.insert(elem);
    allocs_.Insert(elem.first);
  }
  allocs_.Union(gen.Allocs);
}

// -----------------------------------------------------------------------------
void Analysis::LiveSet::Union(const LiveSet &that)
{
  for (auto elem : that.elems_) {
    elems_.insert(elem);
    allocs_.Insert(elem.first);
  }
  allocs_.Union(that.allocs_);
}

// -----------------------------------------------------------------------------
void Analysis::ReachabilityGen::Minus(const ReachabilityKill &kill)
{
  for (auto it = Elems.begin(); it != Elems.end(); ) {
    if (kill.Elems.count(it->first)) {
      it = Elems.erase(it);
      continue;
    }
    if (kill.Allocs.Contains(it->first.first)) {
      it = Elems.erase(it);
      continue;
    }
    ++it;
  }
}

// -----------------------------------------------------------------------------
void Analysis::ReachabilityGen::Union(const ReachabilityGen &gen)
{
  for (auto &elem : gen.Elems) {
    auto it = Elems.insert(elem);
    if (!it.second) {
      it.first->second = elem.second;
    }
  }
}

// -----------------------------------------------------------------------------
void Analysis::ReachabilityKill::Union(const ReachabilityKill &kill)
{
  Allocs.Union(kill.Allocs);
  for (auto &elem : kill.Elems) {
    Elems.insert(elem);
  }
}
