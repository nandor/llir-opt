// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/inst.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/analysis/live_variables.h"


// -----------------------------------------------------------------------------
LiveVariables::LiveVariables(const Func *func)
  : loops_(func)
{
  TraverseDAG(&func->getEntryBlock());
  for (auto *loop : loops_) {
    TraverseLoop(loop);
  }
}

// -----------------------------------------------------------------------------
LiveVariables::~LiveVariables()
{
}


// -----------------------------------------------------------------------------
std::vector<ConstRef<Inst>> LiveVariables::LiveOut(const Inst *inst)
{
  const Block *block = inst->getParent();
  if (block != liveBlock_) {
    liveBlock_ = block;
    liveCache_.clear();

    auto &info = live_[block];

    InstSet live = info.second;
    for (auto it = block->rbegin(); it != block->rend(); ++it) {
      liveCache_[&*it] = live;
      KillDef(live, &*it);
    }
  }

  auto &cached = liveCache_[inst];
  std::vector<ConstRef<Inst>> ordered;
  std::copy(cached.begin(), cached.end(), std::back_inserter(ordered));
  std::sort(
      ordered.begin(),
      ordered.end(),
      [](ConstRef<Inst> a, ConstRef<Inst> b)
      {
        return a->GetOrder() < b->GetOrder();
      }
  );
  return ordered;
}

// -----------------------------------------------------------------------------
void LiveVariables::TraverseDAG(const Block *block)
{
  // Process children.
  for (auto *succ : block->successors()) {
    if (!loops_.IsLoopEdge(block, succ)) {
      auto *node = loops_.HighestAncestor(block, succ);
      if (!live_.count(node)) {
        TraverseDAG(node);
      }
    }
  };

  // liveOut = PhiUses(block)
  InstSet liveOut;
  for (auto *succ : block->successors()) {
    for (auto &phi : succ->phis()) {
      liveOut.insert(phi.GetValue(block));
    }
  }

  // Merge liveIn infos of children.
  for (auto *succ : block->successors()) {
    if (!loops_.IsLoopEdge(block, succ)) {
      auto *node = loops_.HighestAncestor(block, succ);
      // liveOut = liveOut U (LiveIn(S) \ PhiDefs(S))
      InstSet live = live_[node].first;
      for (auto &phi : node->phis()) {
        live.erase(&phi);
      }

      liveOut.insert(live.begin(), live.end());
    }
  }

  // LiveOut(B) = liveOut
  InstSet liveIn(liveOut);
  for (auto it = block->rbegin(); it != block->rend(); ++it) {
    if (it->Is(Inst::Kind::PHI)) {
      break;
    }
    KillDef(liveIn, &*it);
  }

  // LiveIn(B) = Live U PhiDefs(B)
  for (auto &phi : block->phis()) {
    liveIn.insert(&phi);
  }

  // Record liveness for this block.
  live_.emplace(block, std::make_pair(liveIn, liveOut));
}

// -----------------------------------------------------------------------------
void LiveVariables::TraverseLoop(LoopNesting::Loop *loop)
{
  auto *header = loop->GetHeader();

  // liveLoop = LiveIn(header) \ PhiDefs(header)
  InstSet liveLoop = live_[header].first;
  for (auto &phi : header->phis()) {
    liveLoop.erase(&phi);
  }

  for (auto *innerBlock : loop->blocks()) {
    auto &[liveIn, liveOut] = live_[innerBlock];
    for (auto &inst : liveLoop) {
      liveIn.insert(inst);
      liveOut.insert(inst);
    }
  }

  for (auto *innerLoop : loop->loops()) {
    auto *innerHeader = innerLoop->GetHeader();
    auto &[liveIn, liveOut] = live_[innerHeader];
    for (auto &inst : liveLoop) {
      liveIn.insert(inst);
      liveOut.insert(inst);
    }
    TraverseLoop(innerLoop);
  }
}

// -----------------------------------------------------------------------------
void LiveVariables::KillDef(InstSet &live, const Inst *inst)
{
  if (inst->Is(Inst::Kind::ARG)) {
    // Argument instructions do not kill - they must be live on entry.
    return;
  }

  for (unsigned i = 0, n = inst->GetNumRets(); i < n; ++i) {
    live.erase(ConstRef<Inst>(inst, i));
  }

  for (ConstRef<Value> value : inst->operand_values()) {
    if (ConstRef<Inst> inst = ::cast_or_null<Inst>(value)) {
      live.insert(inst);
    }
  }
}
