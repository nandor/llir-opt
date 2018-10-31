// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Pass.h>



/**
 * Custom pass to generate MIR from GenM IR instead of LLVM IR.
 */
class X86ISel final : public llvm::ModulePass {
public:
  static char ID;
  X86ISel() : ModulePass(ID) { }

private:
  bool runOnModule(llvm::Module &M) override;

  llvm::StringRef getPassName() const override;

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
};
