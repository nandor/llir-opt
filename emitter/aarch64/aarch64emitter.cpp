// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetLoweringObjectFile.h>

#include "core/func.h"
#include "core/prog.h"
#include "core/visibility.h"
#include "core/target/aarch64.h"
#include "emitter/aarch64/aarch64annot_printer.h"
#include "emitter/aarch64/aarch64isel.h"
#include "emitter/aarch64/aarch64emitter.h"
#include "emitter/aarch64/aarch64runtime_printer.h"

#define DEBUG_TYPE "llir-aarch64-isel-pass"



// -----------------------------------------------------------------------------
AArch64Emitter::AArch64Emitter(
    const std::string &path,
    llvm::raw_fd_ostream &os,
    AArch64Target &target)
  : Emitter(path, os, target)
  , TLII_(target.GetTriple())
  , LibInfo_(TLII_)
{
  // Look up a backend for this target.
  std::string error;
  auto *llvmTarget = llvm::TargetRegistry::lookupTarget(triple_, error);
  if (!llvmTarget) {
    llvm::report_fatal_error(error);
  }

  // Initialise the target machine. Hacky cast to expose LLVMTargetMachine.
  llvm::TargetOptions opt;
  opt.MCOptions.AsmVerbose = true;
  TM_ = static_cast<llvm::AArch64TargetMachine *>(
      llvmTarget->createTargetMachine(
          triple_,
          target.getCPU(),
          target.getFS(),
          opt,
          llvm::Reloc::Model::PIC_,
          llvm::CodeModel::Small,
          llvm::CodeGenOpt::Aggressive
      )
  );
  TM_->setFastISel(false);
}

// -----------------------------------------------------------------------------
AArch64Emitter::~AArch64Emitter()
{
}

// -----------------------------------------------------------------------------
ISel *AArch64Emitter::CreateISelPass(
    const Prog &prog,
    llvm::CodeGenOpt::Level opt)
{
  return new AArch64ISel(
      target_,
      *TM_,
      LibInfo_,
      prog,
      llvm::CodeGenOpt::Aggressive,
      shared_
  );
}

// -----------------------------------------------------------------------------
AnnotPrinter *AArch64Emitter::CreateAnnotPass(
    llvm::MCContext &mcCtx,
    llvm::MCStreamer &mcStreamer,
    const llvm::TargetLoweringObjectFile &objInfo,
    ISel &isel)
{
  return new AArch64AnnotPrinter(
      &mcCtx,
      &mcStreamer,
      &objInfo,
      TM_->createDataLayout(),
      isel,
      shared_
  );
}

// -----------------------------------------------------------------------------
llvm::ModulePass *AArch64Emitter::CreateRuntimePass(
    const Prog &prog,
    llvm::MCContext &mcCtx,
    llvm::MCStreamer &mcStreamer,
    const llvm::TargetLoweringObjectFile &objInfo)
{
  return new AArch64RuntimePrinter(
      prog,
      *TM_,
      mcCtx,
      mcStreamer,
      objInfo,
      shared_
  );
}
