// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Target/RISCV/RISCVSubtarget.h>
#include <llvm/Target/RISCV/RISCVTargetMachine.h>

#include "emitter/emitter.h"

class Func;



/**
 * Direct RISCV emitter.
 */
class RISCVEmitter : public Emitter {
public:
  /// Creates an x86 emitter.
  RISCVEmitter(
      const std::string &path,
      llvm::raw_fd_ostream &os,
      const std::string &triple,
      const std::string &cpu,
      const std::string &tuneCPU,
      const std::string &fs,
      const std::string &abi,
      bool shared
  );
  /// Destroys the x86 emitter.
  ~RISCVEmitter() override;

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
  /// LLVM Target.
  const llvm::Target *target_;
  /// LLVM target library info.
  llvm::TargetLibraryInfoImpl TLII_;
  /// LLVM target library info.
  llvm::TargetLibraryInfo LibInfo_;
  /// LLVM target machine.
  std::unique_ptr<llvm::RISCVTargetMachine> TM_;
};
