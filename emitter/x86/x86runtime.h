// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Pass.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/Target/X86/X86Subtarget.h>

class Prog;
class Data;



/**
 * Pass to print runtime methods to the output object.
 */
class X86Runtime final : public llvm::ModulePass {
public:
  static char ID;

  /// Initialises the pass which prints data sections.
  X86Runtime(
      const Prog &Prog,
      llvm::MCContext *ctx,
      llvm::MCStreamer *os,
      const llvm::MCObjectFileInfo *objInfo,
      const llvm::DataLayout &layout,
      const llvm::X86Subtarget &sti,
      bool shared
  );

private:
  /// Creates MachineFunctions from LLIR IR.
  bool runOnModule(llvm::Module &M) override;
  /// Hardcoded name.
  llvm::StringRef getPassName() const override;
  /// Requires MachineModuleInfo.
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
  /// Emits caml_call_gc
  void EmitCamlCallGc();
  /// Emits caml_c_call
  void EmitCamlCCall();
  /// Emit caml_alloc{1,2,3,N}
  void EmitCamlAlloc(const std::optional<unsigned> N);
  /// Lowers a symbol name.
  llvm::MCSymbol *LowerSymbol(const std::string &name);
  /// Lowers a symbol to an expression.
  llvm::MCOperand LowerOperand(const std::string &name, unsigned Offset = 0);
  /// Lowers a symbol to an expression.
  llvm::MCOperand LowerOperand(llvm::MCSymbol *symbol, unsigned Offset = 0);

  /// Lowers an instruction to fetch Caml_state.
  void LowerCamlState(unsigned reg);
  /// Lowers a store to memory.
  void LowerStore(unsigned Reg, unsigned state, const std::string &name);
  /// Lowers a load from memory.
  void LowerLoad(unsigned Reg, unsigned state, const std::string &name);
  /// Adds a rip-relative address to an instruction.
  void AddAddr(llvm::MCInst &MI, unsigned state, const std::string &name);

private:
  /// Program to print.
  const Prog &prog_;
  /// LLVM context.
  llvm::MCContext *ctx_;
  /// Streamer to emit output to.
  llvm::MCStreamer *os_;
  /// Object-file specific information.
  const llvm::MCObjectFileInfo *objInfo_;
  /// Data layout.
  const llvm::DataLayout &layout_;
  /// Subtarget info.
  const llvm::X86Subtarget &sti_;
  /// Flag to indicate whether a shared library or a static library is built.
  bool shared_;
};
