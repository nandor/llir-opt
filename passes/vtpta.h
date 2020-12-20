// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_set>

#include "core/analysis.h"



/**
 * Points to Analysis based on [Hardekopf 2007].
 */
class VariantTypePointsToAnalysis final : public Analysis {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  VariantTypePointsToAnalysis(PassManager *passManager)
    : Analysis(passManager)
  {
  }

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;
};

template<> struct AnalysisID<VariantTypePointsToAnalysis> { static char ID; };
