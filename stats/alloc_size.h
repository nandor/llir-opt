// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"
#include "core/insts/call.h"



/**
 * Pass to determine allocation sizes.
 */
class AllocSizePass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  AllocSizePass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Helper to analyse a call.
  void AnalyseCall(CallSite &inst);

  /// Register an allocation of a given size.
  void AnalyseAlloc(const std::optional<int64_t> &size);

private:
  /// Number of known alloc sizes.
  uint64_t numKnownAllocs = 0;
  /// Number of truncated alloc sizes.
  uint64_t numTruncatedAllocs = 0;
  /// Number of unknown alloc sizes.
  uint64_t numUnknownAllocs = 0;
};
