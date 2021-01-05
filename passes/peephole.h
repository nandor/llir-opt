// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"
#include "core/inst_visitor.h"

class Func;
class MovInst;



/**
 * Pass to eliminate unnecessary moves.
 */
class PeepholePass final : public Pass, InstVisitor<bool> {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  PeepholePass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  bool VisitInst(Inst &inst) override { return false; }
  bool VisitAddInst(AddInst &inst) override;
  bool VisitSubInst(SubInst &inst) override;
};
