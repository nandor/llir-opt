// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Pass.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/Target/X86/X86Subtarget.h>

#include "emitter/runtime_printer.h"

class Prog;
class Data;



/**
 * Pass to print runtime methods to the output object.
 */
class X86RuntimePrinter final : public RuntimePrinter {
public:
  static char ID;

  /// Initialises the pass which prints data sections.
  X86RuntimePrinter(
      const Prog &Prog,
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
  llvm::MCSymbol *LowerSymbol(const std::string &name);
  /// Lowers a symbol to an expression.
  llvm::MCOperand LowerOperand(const std::string &name, unsigned Offset = 0);
  /// Lowers a symbol to an expression.
  llvm::MCOperand LowerOperand(llvm::MCSymbol *symbol, unsigned Offset = 0);
  /// Lowers an instruction to fetch Caml_state.
  void LowerCamlState(unsigned reg, const llvm::X86Subtarget &sti);
  /// Lowers a store to memory.
  void LowerStore(
      unsigned Reg,
      unsigned state,
      const std::string &name,
      const llvm::X86Subtarget &sti
  );
  /// Lowers a load from memory.
  void LowerLoad(
      unsigned Reg,
      unsigned state,
      const std::string &name,
      const llvm::X86Subtarget &sti
  );
  /// Adds a rip-relative address to an instruction.
  void AddAddr(llvm::MCInst &MI, unsigned state, const std::string &name);
};
