// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Target/AArch64/AArch64Subtarget.h>
#include <llvm/Target/AArch64/AArch64TargetMachine.h>

#include "emitter/emitter.h"

class Func;



/**
 * Direct AArch64 emitter.
 */
class AArch64Emitter : public Emitter {
public:
  /// Creates an x86 emitter.
  AArch64Emitter(
      const std::string &path,
      llvm::raw_fd_ostream &os,
      AArch64Target &target
  );
  /// Destroys the x86 emitter.
  ~AArch64Emitter() override;

protected:
  /// Returns the generic target machine.
  llvm::LLVMTargetMachine &GetTargetMachine() override { return *TM_; }
  /// Creates the LLIR-to-SelectionDAG pass.
  ISel *CreateISelPass(
      const Prog &prog,
      llvm::CodeGenOpt::Level opt
  ) override;
  /// Creates the annotation generation pass.
  AnnotPrinter *CreateAnnotPass(
      llvm::MCContext &mcCtx,
      llvm::MCStreamer &mcStreamer,
      const llvm::TargetLoweringObjectFile &objInfo,
      ISel &isel
  ) override;
  /// Creates the runtime generation pass.
  llvm::ModulePass *CreateRuntimePass(
      const Prog &prog,
      llvm::MCContext &mcCtx,
      llvm::MCStreamer &mcStreamer,
      const llvm::TargetLoweringObjectFile &objInfo
  ) override;

private:
  /// LLVM target library info.
  llvm::TargetLibraryInfoImpl TLII_;
  /// LLVM target library info.
  llvm::TargetLibraryInfo LibInfo_;
  /// LLVM target machine.
  llvm::AArch64TargetMachine *TM_;
};
