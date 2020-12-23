// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;
class CallSite;



/**
 * Pass to eliminate unnecessary moves.
 */
class SpecialisePass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  SpecialisePass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Helper types to capture specialisation parameters.
  using Param = std::pair<unsigned, Func *>;
  using Params = std::vector<Param>;

  /// Specialises a function, given some parameters.
  Func *Specialise(Func *oldFunc, const Params &params);
  /// Specialises a call site.
  std::pair<std::vector<Ref<Inst>>, std::vector<TypeFlag>>
  Specialise(CallSite *inst, const Params &params);
};