// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;



/**
 * OCaml global simplification
 *
 * If a function is only reached through caml_globals, it can be removed
 * as it is only accessible to the garbage collector root traversal.
 */
class CamlGlobalSimplifyPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  CamlGlobalSimplifyPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Recursively simplify objects starting at caml_globals.
  bool Visit(Object *object);
};
