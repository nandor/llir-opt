// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/target.h"
#include "core/inst_visitor.h"



namespace tags {

class RegisterAnalysis;

class ConstraintSolver : private InstVisitor<void> {
public:
  ConstraintSolver(RegisterAnalysis &analysis, const Target *target, Prog &prog);

  void Solve();

  void VisitSubInst(SubInst &i) override;
  void VisitAddInst(AddInst &i) override;
  void VisitInst(Inst &i) override {}

private:
  /// Reference to the analysis.
  RegisterAnalysis &analysis_;
  /// Reference to target info.
  const Target *target_;
  /// Program to analyse.
  Prog &prog_;
};

}
