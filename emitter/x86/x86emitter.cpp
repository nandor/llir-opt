// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Optional.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/CodeGen/SelectionDAG.h>
#include <llvm/CodeGen/TargetPassConfig.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include "core/func.h"
#include "core/prog.h"
#include "emitter/x86/x86isel.h"
#include "emitter/x86/x86emitter.h"

#define DEBUG_TYPE "genm-isel-pass"

using namespace llvm;



// -----------------------------------------------------------------------------
X86Emitter::X86Emitter(const std::string &path)
  : path_(path)
  , context_()
{
  // Build the target triple.
  auto triple = "x86_64-apple-darwin13.4.0";

  // Look up a backend for this target.
  std::string error;
  target_ = TargetRegistry::lookupTarget(triple, error);
  if (!target_) {
    throw std::runtime_error(error);
  }

  TargetOptions opt;
  targetMachine_ = static_cast<LLVMTargetMachine *>(
      target_->createTargetMachine(
          triple,
          "generic",
          "",
          opt,
          Optional<Reloc::Model>()
      )
  );
  targetMachine_->setFastISel(false);
}

// -----------------------------------------------------------------------------
X86Emitter::~X86Emitter()
{
}

// -----------------------------------------------------------------------------
void X86Emitter::Emit(const Prog *prog)
{
  std::error_code errCode;
  raw_fd_ostream dest(path_, errCode, sys::fs::F_None);
  legacy::PassManager passMngr;

  // Create a machine module info object.
  auto *mmInfo = new MachineModuleInfo(targetMachine_);
  auto *mcCtx = &mmInfo->getContext();
  passMngr.add(mmInfo);

  // Create a target pass configuration.
  auto *passConfig = targetMachine_->createPassConfig(passMngr);
  passConfig->setDisableVerify(false);
  passConfig->setInitialized();
  passConfig->addPass(new X86ISel());
  passMngr.add(passConfig);

  // Add the assembly printer.
  auto type = TargetMachine::CGFT_AssemblyFile;
  if (targetMachine_->addAsmPrinter(passMngr, dest, nullptr, type, *mcCtx)) {
    throw std::runtime_error("Cannot create AsmPrinter");
  }

  // Add a pass to clean up memory.
  passMngr.add(createFreeMachineFunctionPass());

  // Create a dummy module.
  auto module = std::make_unique<Module>(path_, context_);

  // Run all passes and emit code.
  passMngr.run(*module);
  dest.flush();
}
