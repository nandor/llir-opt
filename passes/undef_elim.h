// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"
#include "core/insts.h"
#include "core/inst_visitor.h"

class Func;



/**
 * Pass to simplify the program exploiting undefined behaviour.
 *
 * This pass simplifies control flow instructions, assuming the undefined
 * value is false or zero, turning conditional jumps into unconditional
 * ones, selecting the first successor as the target.
 */
class UndefElimPass final : public Pass, InstVisitor<bool> {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  UndefElimPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Simplifies a conditional jump.
  bool VisitJumpCondInst(JumpCondInst &i) override;
  /// Simplifies a switch instruction.
  bool VisitSwitchInst(SwitchInst &i) override;
  /// Simplifies a select instruction.
  bool VisitSelectInst(SelectInst &i) override;
  /// Simplifies a store instruction.
  bool VisitStoreInst(StoreInst &i) override;
  /// Catch-all case.
  bool VisitInst(Inst &i) override { return false; }
};
