// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/X86/X86Subtarget.h>
#include <llvm/Target/X86/X86TargetMachine.h>

#include "core/target.h"

class Func;



/**
 * X86 target information.
 */
class X86Target final : public Target {
private:
  /// Kind for RTTI.
  static const Kind kKind = Kind::X86;

public:
  X86Target(
      const llvm::Triple &triple,
      const std::string &cpu,
      const std::string &tuneCPU,
      const std::string &fs,
      const std::string &abi,
      bool shared
  );

  /// Returns the generic target machine.
  llvm::X86TargetMachine &GetTargetMachine() { return *machine_; }

  /// Returns the subtarget.
  const llvm::X86Subtarget &GetSubtarget(const Func &func) const;

private:
  friend class Target;
  /// LLVM target machine.
  std::unique_ptr<llvm::X86TargetMachine> machine_;
};
