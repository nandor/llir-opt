// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;



/**
 * Pass to eliminate unnecessary moves.
 */
class DedupBlockPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  DedupBlockPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Deduplicates blocks with no successors.
  bool DedupExits(Func &func);
  /// Deduplicates a block.
  void DedupBlock(const Func *func, const Block *block);

  using InstMap = llvm::DenseMap<const Inst *, const Inst *>;
  /// Checks if an exit block is the duplicate of another.
  bool IsDuplicateExit(const Block *b1, const Block *b2);
  /// Checks if two instructions are equal.
  bool IsEqual(const Inst *i1, const Inst *i2, InstMap &insts);
};
