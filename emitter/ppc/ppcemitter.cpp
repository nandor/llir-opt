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
#include "core/target/ppc.h"
#include "emitter/ppc/ppcannot_printer.h"
#include "emitter/ppc/ppcisel.h"
#include "emitter/ppc/ppcemitter.h"
#include "emitter/ppc/ppcruntime_printer.h"

#define DEBUG_TYPE "llir-ppc-isel-pass"



// -----------------------------------------------------------------------------
std::string GetFeatures(llvm::StringRef fs)
{
  if (fs.empty()) {
    return "+crbits,+64bit";
  } else {
    return (fs + ",+crbits,+64bit").str();
  }
}

// -----------------------------------------------------------------------------
PPCEmitter::PPCEmitter(
    const std::string &path,
    llvm::raw_fd_ostream &os,
    PPCTarget &target)
  : Emitter(path, os, target)
  , TLII_(target.GetTriple())
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
  TM_.reset(static_cast<llvm::PPCTargetMachine *>(
      llvmTarget->createTargetMachine(
          triple_,
          target.getCPU(),
          GetFeatures(target.getFS()),
          opt,
          llvm::Reloc::Model::Static,
          llvm::CodeModel::Medium,
          llvm::CodeGenOpt::Aggressive
      )
  ));
  TM_->setFastISel(false);
}

// -----------------------------------------------------------------------------
PPCEmitter::~PPCEmitter()
{
}

// -----------------------------------------------------------------------------
ISel *PPCEmitter::CreateISelPass(
    const Prog &prog,
    llvm::CodeGenOpt::Level opt)
{
  return new PPCISel(
      target_,
      *TM_,
      LibInfo_,
      prog,
      llvm::CodeGenOpt::Aggressive,
      shared_
  );
}

// -----------------------------------------------------------------------------
AnnotPrinter *PPCEmitter::CreateAnnotPass(
    llvm::MCContext &mcCtx,
    llvm::MCStreamer &mcStreamer,
    const llvm::TargetLoweringObjectFile &objInfo,
    ISel &isel)
{
  return new PPCAnnotPrinter(
      &mcCtx,
      &mcStreamer,
      &objInfo,
      TM_->createDataLayout(),
      isel,
      shared_
  );
}

// -----------------------------------------------------------------------------
llvm::ModulePass *PPCEmitter::CreateRuntimePass(
    const Prog &prog,
    llvm::MCContext &mcCtx,
    llvm::MCStreamer &mcStreamer,
    const llvm::TargetLoweringObjectFile &objInfo)
{
  return new PPCRuntimePrinter(
      prog,
      *TM_,
      mcCtx,
      mcStreamer,
      objInfo,
      shared_
  );
}
