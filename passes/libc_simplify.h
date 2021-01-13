// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;



/**
 * Pass to eliminate unnecessary moves.
 */
class LibCSimplifyPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  LibCSimplifyPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Helper to iterate over calls to a method.
  void Simplify(
      Global *g,
      std::optional<Inst *> (LibCSimplifyPass::*f)(CallSite &)
  );
  /// Simplify calls to free.
  std::optional<Inst *> SimplifyFree(CallSite &call);
  /// Simplify calls to strlen.
  std::optional<Inst *> SimplifyStrlen(CallSite &call);
};
