// This file if part of the genm-opt project.
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
std::set<const Inst *> LiveVariables::LiveOut(const Inst *inst)
{
  auto &info = live_[inst->getParent()];

  std::set<const Inst *> live = info.second;
  for (auto it = inst->getParent()->rbegin(); &*it != inst; ++it) {
    KillDef(live, &*it);
  }

  return live;
}

// -----------------------------------------------------------------------------
void LiveVariables::TraverseDAG(const Block *block)
{
  // Process children.
  for (auto *succ : block->successors()) {
    if (!loops_.IsLoopEdge(block, succ)) {
      if (!live_.count(succ)) {
        TraverseDAG(succ);
      }
    }
  };

  // liveOut = PhiUses(block)
  std::set<const Inst *> liveOut;
  for (auto *succ : block->successors()) {
    for (auto &phi : succ->phis()) {
      if (auto *inst = ::dyn_cast_or_null<Inst>(phi.GetValue(block))) {
        liveOut.insert(inst);
      }
    }
  }

  // Merge liveIn infos of children.
  for (auto *succ : block->successors()) {
    if (!loops_.IsLoopEdge(block, succ)) {
      // liveOut = liveOut U (LiveIn(S) \ PhiDefs(S))
      std::set<const Inst *> live = live_[succ].first;
      for (auto &phi : succ->phis()) {
        live.erase(&phi);
      }

      liveOut.insert(live.begin(), live.end());
    }
  }

  // LiveOut(B) = liveOut
  std::set<const Inst *> liveIn(liveOut);
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
  /*
  llvm::errs() << block->getName() << "\n";

  llvm::errs() << "\t";
  for (auto *i : liveIn) {
    p.Print(static_cast<const Value *>(i));
    llvm::errs() << " ";
  }
  llvm::errs() << "\n";

  llvm::errs() << "\t";
  for (auto *i : liveOut) {
    p.Print(static_cast<const Value *>(i));
    llvm::errs() << " ";
  }
  llvm::errs() << "\n";
  */
  // Record liveness for this block.
  live_.emplace(block, std::make_pair(liveIn, liveOut));
}

// -----------------------------------------------------------------------------
void LiveVariables::TraverseLoop(LoopNesting::Loop *loop)
{
  auto *header = loop->GetHeader();

  // liveLoop = LiveIn(header) \ PhiDefs(header)
  std::set<const Inst *> liveLoop = live_[header].first;
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
void LiveVariables::KillDef(std::set<const Inst *> &live, const Inst *inst)
{
  if (inst->GetNumRets()) {
    live.erase(inst);
  }

  for (auto *value : inst->operand_values()) {
    if (auto *inst = ::dyn_cast_or_null<Inst>(value)) {
      live.insert(inst);
    }
  }
}
