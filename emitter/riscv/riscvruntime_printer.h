// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Pass.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/Target/RISCV/RISCVSubtarget.h>

#include "emitter/runtime_printer.h"

class Prog;
class Data;



/**
 * Pass to print runtime methods to the output object.
 */
class RISCVRuntimePrinter final : public RuntimePrinter {
public:
  static char ID;

  /// Initialises the pass which prints data sections.
  RISCVRuntimePrinter(
      const Prog &prog,
      const llvm::TargetMachine &tm,
      llvm::MCContext &ctx,
      llvm::MCStreamer &os,
      const llvm::MCObjectFileInfo &objInfo,
      bool shared
  );

private:
  /// Hardcoded name.
  llvm::StringRef getPassName() const override;

private:
  /// Emits caml_call_gc
  void EmitCamlCallGc(llvm::Function &F) override;
  /// Emits caml_c_call
  void EmitCamlCCall(llvm::Function &F) override;

private:
  /// Lowers a symbol name.
  llvm::MCSymbol *LowerSymbol(const char *name);
  /// Load the GC state.
  void LoadCamlState(
      llvm::Register state,
      const llvm::RISCVSubtarget &sti
  );
  /// Stores to a state variable.
  void StoreState(
      llvm::Register state,
      llvm::Register val,
      const char *name,
      const llvm::RISCVSubtarget &sti
  );
  /// Loads from a state variable.
  void LoadState(
      llvm::Register state,
      llvm::Register val,
      const char *name,
      const llvm::RISCVSubtarget &sti
  );
};
