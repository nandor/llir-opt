// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/CodeGen.h>
#include <llvm/CodeGen/GlobalISel/CallLowering.h>
#include <llvm/CodeGen/GlobalISel/InstructionSelect.h>
#include <llvm/CodeGen/GlobalISel/RegisterBankInfo.h>
#include <llvm/CodeGen/GlobalISel/LegalizerInfo.h>
#include <llvm/Target/X86/X86Subtarget.h>
#include <llvm/Target/X86/X86TargetMachine.h>

#include "core/target/x86.h"
#include "emitter/emitter.h"

class Func;



/**
 * Direct X86 emitter.
 */
class X86Emitter : public Emitter {
public:
  /// Creates an x86 emitter.
  X86Emitter(
      const std::string &path,
      llvm::raw_fd_ostream &os,
      X86Target &target
  );
  /// Destroys the x86 emitter.
  ~X86Emitter() override;

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
  std::unique_ptr<llvm::X86TargetMachine> TM_;
};
