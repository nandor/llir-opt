// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Target/PowerPC/PPCSubtarget.h>
#include <llvm/Target/PowerPC/PPCTargetMachine.h>

#include "emitter/emitter.h"

class Func;



/**
 * Direct PPC emitter.
 */
class PPCEmitter : public Emitter {
public:
  /// Creates an x86 emitter.
  PPCEmitter(
      const std::string &path,
      llvm::raw_fd_ostream &os,
      PPCTarget &target
  );
  /// Destroys the x86 emitter.
  ~PPCEmitter() override;

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
  std::unique_ptr<llvm::PPCTargetMachine> TM_;
};
