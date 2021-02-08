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
#include "core/target/riscv.h"
#include "emitter/riscv/riscvannot_printer.h"
#include "emitter/riscv/riscvisel.h"
#include "emitter/riscv/riscvemitter.h"
#include "emitter/riscv/riscvruntime_printer.h"

#define DEBUG_TYPE "llir-riscv-isel-pass"



// -----------------------------------------------------------------------------
RISCVEmitter::RISCVEmitter(
    const std::string &path,
    llvm::raw_fd_ostream &os,
    RISCVTarget &target)
  : Emitter(path, os, target)
  , TLII_(llvm::Triple(target.GetTriple()))
  , LibInfo_(TLII_)
{
  // Look up a backend for this target.
  std::string error;
  auto llvmTarget = llvm::TargetRegistry::lookupTarget(triple_, error);
  if (!llvmTarget) {
    llvm::report_fatal_error(error);
  }

  // Initialise the target machine. Hacky cast to expose LLVMTargetMachine.
  llvm::TargetOptions opt;
  opt.MCOptions.AsmVerbose = true;
  opt.MCOptions.ABIName = target.getABI();
  opt.FunctionSections = true;
  TM_.reset(static_cast<llvm::RISCVTargetMachine *>(
      llvmTarget->createTargetMachine(
          triple_,
          target.getCPU(),
          target.getFS(),
          opt,
          llvm::Reloc::Model::PIC_,
          llvm::CodeModel::Small,
          llvm::CodeGenOpt::Aggressive
      )
  ));
  TM_->setFastISel(false);
}

// -----------------------------------------------------------------------------
RISCVEmitter::~RISCVEmitter()
{
}

// -----------------------------------------------------------------------------
ISel *RISCVEmitter::CreateISelPass(
    const Prog &prog,
    llvm::CodeGenOpt::Level opt)
{
  return new RISCVISel(
      *TM_,
      LibInfo_,
      prog,
      llvm::CodeGenOpt::Aggressive,
      shared_
  );
}

// -----------------------------------------------------------------------------
AnnotPrinter *RISCVEmitter::CreateAnnotPass(
    llvm::MCContext &mcCtx,
    llvm::MCStreamer &mcStreamer,
    const llvm::TargetLoweringObjectFile &objInfo,
    ISel &isel)
{
  return new RISCVAnnotPrinter(
      &mcCtx,
      &mcStreamer,
      &objInfo,
      TM_->createDataLayout(),
      isel,
      shared_
  );
}

// -----------------------------------------------------------------------------
llvm::ModulePass *RISCVEmitter::CreateRuntimePass(
    const Prog &prog,
    llvm::MCContext &mcCtx,
    llvm::MCStreamer &mcStreamer,
    const llvm::TargetLoweringObjectFile &objInfo)
{
  return new RISCVRuntimePrinter(
      prog,
      *TM_,
      mcCtx,
      mcStreamer,
      objInfo,
      shared_
  );
}
