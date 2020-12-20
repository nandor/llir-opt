// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;



/**
 * Pass to eliminate redundant trampolines.
 */
class SimplifyTrampolinePass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  SimplifyTrampolinePass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;
};
