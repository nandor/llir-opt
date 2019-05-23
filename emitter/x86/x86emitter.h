// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <fstream>
#include <string>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/X86/X86Subtarget.h>
#include <llvm/Target/X86/X86TargetMachine.h>
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
      const std::string &triple);
  /// Destroys the x86 emitter.
  ~X86Emitter() override;

protected:
  void Emit(
      llvm::TargetMachine::CodeGenFileType type,
      const Prog *prog
  ) override;

private:
  /// Path to the output file.
  const std::string path_;
  /// Output stream.
  llvm::raw_fd_ostream &os_;

  /// Target triple.
  const std::string triple_;
  /// LLVM Context.
  llvm::LLVMContext context_;
  /// LLVM Target.
  const llvm::Target *target_;
  /// LLVM target library info.
  llvm::TargetLibraryInfoImpl TLII_;
  /// LLVM target library info.
  llvm::TargetLibraryInfo LibInfo_;
  /// LLVM target machine.
  llvm::X86TargetMachine *TM_;
  /// LLVM subtarget.
  llvm::X86Subtarget *STI_;
};
