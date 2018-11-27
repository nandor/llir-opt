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
#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "emitter/x86/x86isel.h"
#include "emitter/x86/x86emitter.h"

#define DEBUG_TYPE "genm-isel-pass"

using namespace llvm;



// -----------------------------------------------------------------------------
X86Emitter::X86Emitter(const std::string &path)
  : path_(path)
  , triple_("x86_64-apple-darwin13.4.0")
  , context_()
  , TLII_(Triple(triple_))
  , LibInfo_(TLII_)
{
  // Look up a backend for this target.
  std::string error;
  target_ = TargetRegistry::lookupTarget(triple_, error);
  if (!target_) {
    throw std::runtime_error(error);
  }

  // Initialise the target machine. Hacky cast to expose LLVMTargetMachine.
  TargetOptions opt;
  TM_ = static_cast<X86TargetMachine *>(
      target_->createTargetMachine(
          triple_,
          "generic",
          "",
          opt,
          Optional<Reloc::Model>(),
          CodeModel::Small,
          CodeGenOpt::Aggressive
      )
  );
  TM_->setFastISel(false);

  /// Initialise the subtarget.
  STI_ = new X86Subtarget(
      Triple(triple_),
      "",
      "",
      *TM_,
      0,
      0,
      UINT32_MAX
  );
}

// -----------------------------------------------------------------------------
X86Emitter::~X86Emitter()
{
}

// -----------------------------------------------------------------------------
void X86Emitter::Emit(TargetMachine::CodeGenFileType type, const Prog *prog)
{
  std::error_code errCode;
  raw_fd_ostream dest(path_, errCode, sys::fs::F_None);
  legacy::PassManager passMngr;

  // Create a machine module info object.
  auto *MMI = new MachineModuleInfo(TM_);
  auto *MC = &MMI->getContext();

  // Create a target pass configuration.
  auto *passConfig = TM_->createPassConfig(passMngr);
  passMngr.add(passConfig);
  passMngr.add(MMI);

  passConfig->setDisableVerify(false);
  passConfig->addPass(new X86ISel(
      TM_,
      STI_,
      STI_->getInstrInfo(),
      STI_->getRegisterInfo(),
      STI_->getTargetLowering(),
      &LibInfo_,
      prog,
      CodeGenOpt::Aggressive
  ));
  passConfig->addMachinePasses();
  passConfig->setInitialized();

  // Add the assembly printer.
  if (TM_->addAsmPrinter(passMngr, dest, nullptr, type, *MC)) {
    throw std::runtime_error("Cannot create AsmPrinter");
  }

  // Add a pass to clean up memory.
  passMngr.add(createFreeMachineFunctionPass());

  // Create a dummy module.
  auto M = std::make_unique<Module>(path_, context_);

  // Run all passes and emit code.
  passMngr.run(*M);
  dest.flush();
}
