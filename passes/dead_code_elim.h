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
  void Run(Prog *prog) override;
};
