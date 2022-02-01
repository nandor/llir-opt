// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/TargetRegistry.h>

#include "core/func.h"
#include "core/target/x86.h"




// -----------------------------------------------------------------------------
X86Target::X86Target(
    const llvm::Triple &triple,
    const std::string &cpu,
    const std::string &tuneCPU,
    const std::string &fs,
    const std::string &abi,
    bool shared)
  : Target(
      kKind,
      triple,
      cpu.empty() ? "generic" : cpu,
      tuneCPU,
      fs,
      abi,
      shared
    )
{
  // Look up a backend for this target.
  std::string err;
  auto *llvmTarget = llvm::TargetRegistry::lookupTarget(triple.normalize(), err);
  if (!llvmTarget) {
    llvm::report_fatal_error(err);
  }

  // Initialise the target machine. Hacky cast to expose LLVMTargetMachine.
  llvm::TargetOptions opt;
  opt.MCOptions.AsmVerbose = true;
  machine_.reset(static_cast<llvm::X86TargetMachine *>(
      llvmTarget->createTargetMachine(
          triple.str(),
          getCPU(),
          getFS(),
          opt,
          llvm::Reloc::PIC_,
          llvm::CodeModel::Small,
          llvm::CodeGenOpt::Aggressive
      )
  ));
  machine_->setFastISel(false);
}

// -----------------------------------------------------------------------------
const llvm::X86Subtarget &X86Target::GetSubtarget(const Func &func) const
{
  llvm::StringRef cpu = func.getCPU();
  llvm::StringRef tuneCPU = func.getTuneCPU();
  llvm::StringRef fs = func.getFeatures();
  return *machine_->getSubtarget(
      cpu.empty() ? getCPU() : cpu,
      tuneCPU.empty() ? getTuneCPU() : tuneCPU,
      fs.empty() ? getFS() : fs
  );
}
