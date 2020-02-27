// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_set>

#include "core/analysis.h"



/**
 * Points to Analysis based on [Hardekopf 2007].
 */
class PointsToAnalysis final : public Analysis {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  PointsToAnalysis(PassManager *passManager) : Analysis(passManager) {}

  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

  /// Returns the set of functions pointed to.
  bool IsReachable(Func *func) { return reachable_.count(func) != 0; }

private:
  /// Some root nodes for queriable points-to sets.
  std::unordered_set<Func *> reachable_;
};

template<> struct AnalysisID<PointsToAnalysis> { static char ID; };
