// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"



// Analysis identifier.
template<typename T> struct AnalysisID;



/**
 * Base class for analysis passes.
 */
class Analysis : public Pass {
public:
  /// Base class of analyses.
  Analysis(PassManager *passManager);
};
