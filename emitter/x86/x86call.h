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
class X86Call : public CallLowering {
public:
  /// Analyses a function for arguments.
  X86Call(const Func *func) : CallLowering(func) {}
  /// Analyses a call site.
  X86Call(const CallSite *inst) : CallLowering(inst) {}
  /// Analyses a return site.
  X86Call(const ReturnInst *inst) : CallLowering(inst) {}
  /// Analyses a raise site.
  X86Call(const RaiseInst *inst) : CallLowering(inst) {}
  /// Analyses a landing pad.
  X86Call(const LandingPadInst *inst) : CallLowering(inst) {}

  /// Returns the number of bytes allocated on the stack.
  unsigned GetFrameSize() const override
  {
    return llvm::alignTo(stack_, maxAlign_);
  }

  /// Returns unused GPRs.
  llvm::ArrayRef<unsigned> GetUnusedGPRs() const;
  /// Returns the used GPRs.
  llvm::ArrayRef<unsigned> GetUsedGPRs() const;
  /// Returns unused XMMs.
  llvm::ArrayRef<unsigned> GetUnusedXMMs() const;
  /// Returns the used XMMs.
  llvm::ArrayRef<unsigned> GetUsedXMMs() const;

protected:
  /// Assigns a location to a register.
  void AssignArgReg(ArgLoc &loc, llvm::MVT t, llvm::Register reg);
  /// Assigns a location to the stack.
  void AssignArgStack(ArgLoc &loc, llvm::MVT t, unsigned size);
  /// Assigns a location to the stack.
  void AssignArgByVal(ArgLoc &loc, llvm::MVT t, unsigned size, llvm::Align a);
  /// Assigns a location to a register.
  void AssignRetReg(RetLoc &loc, llvm::MVT t, llvm::Register reg);

protected:
  /// Returns the list of GPR registers.
  virtual llvm::ArrayRef<unsigned> GetGPRs() const = 0;
  /// Returns the list of XMM registers.
  virtual llvm::ArrayRef<unsigned> GetXMMs() const = 0;

protected:
  /// Number of arguments in regular registers.
  unsigned argRegs_ = 0;
  /// Number of arguments in vector registers.
  unsigned argXMMs_ = 0;
  /// Number of returns in regular registers.
  unsigned retRegs_ = 0;
  /// Number of returns in vector registers.
  unsigned retXMMs_ = 0;
  /// Number of returns in floating point registers.
  unsigned retFPs_ = 0;
  /// Number of bytes allocated on the stack.
  unsigned stack_ = 0;
  /// Maximum alignment on the stack.
  llvm::Align maxAlign_ = llvm::Align(8);
};

/**
 * X86-32 calling convention analysis.
 */
class X86_32Call final : public X86Call {
public:
  /// Analyses a function for arguments.
  X86_32Call(const Func *func) : X86Call(func) { AnalyseFunc(func); }
  /// Analyses a call site.
  X86_32Call(const CallSite *inst) : X86Call(inst) { AnalyseCall(inst); }
  /// Analyses a return site.
  X86_32Call(const ReturnInst *inst) : X86Call(inst) { AnalyseReturn(inst); }
  /// Analyses a raise site.
  X86_32Call(const RaiseInst *inst) : X86Call(inst) { AnalyseRaise(inst); }
  /// Analyses a landing pad.
  X86_32Call(const LandingPadInst *inst) : X86Call(inst) { AnalysePad(inst); }

protected:
  /// Returns the list of GPR registers.
  llvm::ArrayRef<unsigned> GetGPRs() const override;
  /// Returns the list of XMM registers.
  llvm::ArrayRef<unsigned> GetXMMs() const override;

private:
  /// Location assignment for C calls.
  void AssignArgC(unsigned i, FlaggedType type) override;
  /// Location assignment for Ocaml calls.
  void AssignArgOCaml(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignArgOCamlAlloc(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to GC trampolines.
  void AssignArgOCamlGc(unsigned i, FlaggedType type) override;
  /// Location assignment for Xen hypercalls.
  void AssignArgXen(unsigned i, FlaggedType type) override;
  /// Location assignment for multiboot.
  void AssignArgMultiboot(unsigned i, FlaggedType type) override;

  /// Location assignment for C calls.
  void AssignRetC(unsigned i, FlaggedType type) override;
  /// Location assignment for Ocaml calls.
  void AssignRetOCaml(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignRetOCamlAlloc(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to GC trampolines.
  void AssignRetOCamlGc(unsigned i, FlaggedType type) override;
  /// Location assignment for Xen hypercalls.
  void AssignRetXen(unsigned i, FlaggedType type) override;
};

/**
 * X86-64 calling convention analysis.
 */
class X86_64Call final : public X86Call {
public:
  /// Analyses a function for arguments.
  X86_64Call(const Func *func) : X86Call(func) { AnalyseFunc(func); }
  /// Analyses a call site.
  X86_64Call(const CallSite *inst) : X86Call(inst) { AnalyseCall(inst); }
  /// Analyses a return site.
  X86_64Call(const ReturnInst *inst) : X86Call(inst) { AnalyseReturn(inst); }
  /// Analyses a raise site.
  X86_64Call(const RaiseInst *inst) : X86Call(inst) { AnalyseRaise(inst); }
  /// Analyses a landing pad.
  X86_64Call(const LandingPadInst *inst) : X86Call(inst) { AnalysePad(inst); }

protected:
  /// Returns the list of GPR registers.
  llvm::ArrayRef<unsigned> GetGPRs() const override;
  /// Returns the list of XMM registers.
  llvm::ArrayRef<unsigned> GetXMMs() const override;

private:
  /// Location assignment for C calls.
  void AssignArgC(unsigned i, FlaggedType type) override;
  /// Location assignment for Ocaml calls.
  void AssignArgOCaml(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignArgOCamlAlloc(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to GC trampolines.
  void AssignArgOCamlGc(unsigned i, FlaggedType type) override;
  /// Location assignment for Xen hypercalls.
  void AssignArgXen(unsigned i, FlaggedType type) override;
  /// Location assignment for multiboot.
  void AssignArgMultiboot(unsigned i, FlaggedType type) override;

  /// Location assignment for C calls.
  void AssignRetC(unsigned i, FlaggedType type) override;
  /// Location assignment for Ocaml calls.
  void AssignRetOCaml(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignRetOCamlAlloc(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to GC trampolines.
  void AssignRetOCamlGc(unsigned i, FlaggedType type) override;
  /// Location assignment for Xen hypercalls.
  void AssignRetXen(unsigned i, FlaggedType type) override;
};
