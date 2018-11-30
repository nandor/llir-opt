// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Pass.h>

class Prog;
class X86ISel;



/**
 * X86 Annotation Handler.
 */
class X86Annot final : public llvm::ModulePass {
public:
  static char ID;

  /// Initialises the pass which prints data sections.
  X86Annot(const X86ISel *isel, const Prog *prog);

private:
  /// Creates MachineFunctions from GenM IR.
  bool runOnModule(llvm::Module &M) override;
  /// Hardcoded name.
  llvm::StringRef getPassName() const override;
  /// Requires MachineModuleInfo.
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
  /// Instruction selector pass containing info for annotations.
  const X86ISel *isel_;
  /// Program being lowered.
  const Prog *prog_;
};
