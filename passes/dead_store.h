// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;



/**
 * Pass to eliminate unnecessary moves.
 */
class DeadStorePass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  DeadStorePass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Eliminate stores shadowed by others.
  bool RemoveLocalDeadStores(Func &prog);
  /// Remove redundant stores.
  bool RemoveTautologicalStores(Prog &prog);
};
