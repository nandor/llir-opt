// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"
#include "core/target.h"



namespace tags {

class TypeAnalysis;

/**
 * Helper to produce the initial types for known values.
 */
class Refine : public ConstInstVisitor<void> {
public:
  Refine(TypeAnalysis &analysis, const Target *target)
    : analysis_(analysis)
    , target_(target)
  {
  }

  void VisitInst(const Inst &i) override {}

private:
  /// Reference to the analysis.
  TypeAnalysis &analysis_;
  /// Reference to target info.
  const Target *target_;
};

} // end namespace
