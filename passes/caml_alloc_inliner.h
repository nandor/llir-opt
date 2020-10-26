// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;
class CallInst;



/**
 * Function inliner pass.
 *
 * This pass eliminated caml_alloc* calls, thus it should be executed after
 * all points-to analyses which rely on the presence of these methods to
 * detect allocation sites.
 */
class CamlAllocInlinerPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  CamlAllocInlinerPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;
};
