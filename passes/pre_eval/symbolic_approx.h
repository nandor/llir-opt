// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"

class SymbolicContext;
class SymbolicHeap;



/**
 * Symbolically evaluate an instruction.
 */
class SymbolicApprox final {
public:
  SymbolicApprox(SymbolicContext &ctx) : ctx_(ctx) {}

  /// Over-approximate the effects of a loop.
  void Approximate(std::vector<Block *> blocks);

private:
  /**
   * Over-approximate the effect of a single short loop.
   */
  void ApproximateShortLoop(Block *block);

private:
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
};
