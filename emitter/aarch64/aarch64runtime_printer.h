// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Pass.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/Target/AArch64/AArch64Subtarget.h>

#include "emitter/runtime_printer.h"

class Prog;
class Data;



/**
 * Pass to print runtime methods to the output object.
 */
class AArch64RuntimePrinter final : public RuntimePrinter {
public:
  static char ID;

  /// Initialises the pass which prints data sections.
  AArch64RuntimePrinter(
      const Prog &Prog,
      llvm::MCContext *ctx,
      llvm::MCStreamer *os,
      const llvm::MCObjectFileInfo *objInfo,
      const llvm::DataLayout &layout,
      const llvm::AArch64Subtarget &sti,
      bool shared
  );

private:
  /// Hardcoded name.
  llvm::StringRef getPassName() const override;

private:
  /// Emits caml_call_gc
  void EmitCamlCallGc() override;
  /// Emits caml_c_call
  void EmitCamlCCall() override;
  /// Emit caml_alloc{1,2,3,N}
  void EmitCamlAlloc(const std::optional<unsigned> N) override;

private:
  /// Subtarget info.
  const llvm::AArch64Subtarget &sti_;
};
