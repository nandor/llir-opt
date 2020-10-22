// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_set>
#include <vector>

#include "core/inst.h"
#include "core/analysis/loop_nesting.h"

class Block;
class Func;
class Inst;



/**
 * Helper class to compute live variable info for a function.
 */
class LiveVariables final {
public:
  /**
   * Computes live variable info for a function.
   */
  LiveVariables(const Func *func);

  /**
   * Cleanup.
   */
  ~LiveVariables();

  /**
   * Returns the set of live-ins at a program point.
   */
  std::vector<ConstRef<Inst>> LiveOut(const Inst *inst);

private:
  using InstSet = std::unordered_set<ConstRef<Inst>>;

  /// DFS over the DAG - CFG minus loop edges.
  void TraverseDAG(const Block *block);
  /// DFS over the loop nesting forest.
  void TraverseLoop(LoopNesting::Loop *loop);
  /// Applies the transfer function of an instruction.
  void KillDef(InstSet &live, const Inst *inst);

private:
  /// Loop nesting forest.
  LoopNesting loops_;
  /// Map of visited nodes.
  std::unordered_map
    < const Block *
    , std::pair<InstSet, InstSet>
    > live_;
  /// Cached live outs.
  std::unordered_map
    < const Inst *
    , InstSet
    > liveCache_;
  /// Block for which LVA info was cached.
  const Block *liveBlock_ = nullptr;
};
