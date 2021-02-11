// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;



/**
 * Pass to eliminate unnecessary moves.
 */
class SimplifyCfgPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  SimplifyCfgPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Eliminate conditional jumps with the same target.
  bool EliminateConditionalJumps(Func &func);
  /// Eliminate phi-only blocks.
  bool EliminatePhiBlocks(Func &func);
  /// Thread jumps.
  bool ThreadJumps(Func &func);
  /// Fold branches with known arguments.
  bool FoldBranches(Func &func);
  /// Remove PHIs with a single incoming node.
  bool RemoveSinglePhis(Func &func);
  /// Merge basic blocks Func *funcinto predecessors if they have only one.
  bool MergeIntoPredecessor(Func &func);
  /// Bypasses redundant comparisons involving PHIs.
  bool BypassPhiCmp(Block &block);
  /// Bypasses redundant comparisons involving PHIs, iterating while changes.
  bool BypassPhiCmps(Func &func);

private:
  /// Runs the pass on a function.
  bool Run(Func &func);
};
