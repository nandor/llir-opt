// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <random>

#include "core/pass.h"

class Func;
class CallInst;



/**
 * Pass to eliminate unnecessary moves.
 */
class ReducePass final : public Pass {
public:
  /// Initialises the pass.
  ReducePass(PassManager *passManager, unsigned seed)
    : Pass(passManager), rand_(seed)
  {}

  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Reduces a call.
  void ReduceCall(CallInst *call);

  /// Reduces a value to undefined.
  void ReduceUndefined(Inst *inst);

  /// Returns a random number in a range.
  unsigned Random(unsigned n);

private:
  /// Random generator.
  std::mt19937 rand_;
};
