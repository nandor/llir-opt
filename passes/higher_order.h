// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;



/**
 * Pass to eliminate unnecessary moves.
 */
class HigherOrderPass final : public Pass {
public:
  /// Initialises the pass.
  HigherOrderPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Helper types to capture specialisation parameters.
  using Param = std::pair<unsigned, Func *>;
  using Params = std::vector<Param>;

  /// Specialises a function, given some parameters.
  Func *Specialise(Func *oldFunc, const Params &params);
  /// Specialises a call site.
  template<typename T>
  std::vector<Inst *> Specialise(T *inst, const Params &params);
};
