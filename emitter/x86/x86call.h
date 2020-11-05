// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/iterator_range.h>

#include "core/inst.h"
#include "emitter/call_lowering.h"

class CallInst;
class InvokeInst;
class TailCallInst;
class ReturnInst;
class RaiseInst;
class Func;



/**
 * X86 calling convention analysis.
 */
class X86Call final : public CallLowering {
public:
  /// Analyses a function for arguments.
  X86Call(const Func *func)
    : CallLowering(func)
  {
    AnalyseFunc(func);
  }

  /// Analyses a call site.
  X86Call(const CallSite *inst)
    : CallLowering(inst)
  {
    AnalyseCall(inst);
  }

  /// Analyses a return site.
  X86Call(const ReturnInst *inst)
    : CallLowering(inst)
  {
    AnalyseReturn(inst);
  }

  /// Analyses a raise site.
  X86Call(const RaiseInst *inst)
    : CallLowering(inst)
  {
    AnalyseRaise(inst);
  }

  /// Returns unused GPRs.
  llvm::ArrayRef<unsigned> GetUnusedGPRs() const;
  /// Returns the used GPRs.
  llvm::ArrayRef<unsigned> GetUsedGPRs() const;
  /// Returns unused XMMs.
  llvm::ArrayRef<unsigned> GetUnusedXMMs() const;
  /// Returns the used XMMs.
  llvm::ArrayRef<unsigned> GetUsedXMMs() const;

private:
  /// Location assignment for C calls.
  void AssignArgC(unsigned i, Type type, ConstRef<Inst> value) override;
  /// Location assignment for Ocaml calls.
  void AssignArgOCaml(unsigned i, Type type, ConstRef<Inst> value) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignArgOCamlAlloc(unsigned i, Type type, ConstRef<Inst> value) override;
  /// Location assignment for OCaml to GC trampolines.
  void AssignArgOCamlGc(unsigned i, Type type, ConstRef<Inst> value) override;

  /// Location assignment for C calls.
  void AssignRetC(unsigned i, Type type) override;
  /// Location assignment for Ocaml calls.
  void AssignRetOCaml(unsigned i, Type type) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignRetOCamlAlloc(unsigned i, Type type) override;
  /// Location assignment for OCaml to GC trampolines.
  void AssignRetOCamlGc(unsigned i, Type type) override;

  /// Assigns a location to a register.
  void AssignArgReg(unsigned i, llvm::MVT type, llvm::Register reg);
  /// Assigns a location to a register.
  void AssignRetReg(unsigned i, llvm::MVT type, llvm::Register reg);
  /// Assigns a location to the stack.
  void AssignArgStack(unsigned i, llvm::MVT type, unsigned size);

  /// Returns the list of GPR registers.
  llvm::ArrayRef<unsigned> GetGPRs() const;
  /// Returns the list of XMM registers.
  llvm::ArrayRef<unsigned> GetXMMs() const;

private:
  /// Number of arguments in regular registers.
  uint64_t argRegs_ = 0;
  /// Number of arguments in vector registers.
  uint64_t argXMMs_ = 0;
  /// Number of returns in regular registers.
  uint64_t retRegs_ = 0;
  /// Number of returns in vector registers.
  uint64_t retXMMs_ = 0;
  /// Number of returns in floating point registers.
  uint64_t retFPs_ = 0;
};
