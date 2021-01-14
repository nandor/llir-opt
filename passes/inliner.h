// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;
class CallSite;



/**
 * Function inliner pass.
 */
class InlinerPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  InlinerPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Count the number of uses of a function.
  std::pair<unsigned, unsigned> CountUses(const Func &func);
  /// Check whether a function is worth inlining.
  bool CheckGlobalCost(const Func &callee);
  /// Checks whether a function should be inlined into the init path.
  bool CheckInitCost(const CallSite &call, const Func &callee);

private:
  /// Cache of the use counts of functions.
  std::unordered_map<const Func *, std::pair<unsigned, unsigned>> counts_;
};
