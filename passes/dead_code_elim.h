// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"



/**
 * Pass which eliminates unused functions and symbols.
 */
class DeadCodeElimPass final : public Pass {
public:
  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Runs the pass on a function.
  void Run(Func *func);
};
