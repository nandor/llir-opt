// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"



/**
 * Pass to eliminate unnecessary moves.
 *
 * When generating code through LLVM, SelectionDAG cannot pattern match
 * across basic block boundaries. This pass creates additional copies of
 * comparison instructions used by selects in distinct blocks.
 */
class LocalizeSelectPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  LocalizeSelectPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;
};
