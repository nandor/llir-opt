// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;
class CallInst;



/**
 * Function inliner pass.
 */
class InlinerPass final : public Pass {
public:
  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Returns the function called by the instruction.
  Func *GetCallee(Inst *inst);
  /// Checks if the function has a single use.
  bool HasSingleUse(Func *func);

private:
  /// Inlines potential calls in a block.
  bool Inline(Block *block);
};
