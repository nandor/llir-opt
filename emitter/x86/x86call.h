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
class Func;



/**
 * X86 calling convention analysis.
 */
class X86Call final : public CallLowering {
public:
  /// Analyses a function for arguments.
  X86Call(const Func *func)
    : CallLowering(func)
    , regs_(0)
    , xmms_(0)
  {
    AnalyseFunc(func);
  }

  /// Analyses a call site.
  X86Call(const CallSite *call)
    : CallLowering(call)
    , regs_(0)
    , xmms_(0)
  {
    AnalyseCall(call);
  }

  /// Returns unused GPRs.
  llvm::ArrayRef<unsigned> GetUnusedGPRs() const;
  /// Returns the used GPRs.
  llvm::ArrayRef<unsigned> GetUsedGPRs() const;
  /// Returns unused XMMs.
  llvm::ArrayRef<unsigned> GetUnusedXMMs() const;
  /// Returns the used XMMs.
  llvm::ArrayRef<unsigned> GetUsedXMMs() const;

  /// Returns the type of a return value.
  RetLoc Return(Type type) const override;

private:
  /// Location assignment for C calls.
  void AssignC(unsigned i, Type type, const Inst *value) override;
  /// Location assignment for Ocaml calls.
  void AssignOCaml(unsigned i, Type type, const Inst *value) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignOCamlAlloc(unsigned i, Type type, const Inst *value) override;
  /// Location assignment for OCaml to GC trampolines.
  void AssignOCamlGc(unsigned i, Type type, const Inst *value) override;

  /// Assigns a location to a register.
  void AssignReg(unsigned i, Type type, const Inst *value, unsigned reg);
  /// Assigns a location to an XMM register.
  void AssignXMM(unsigned i, Type type, const Inst *value, unsigned reg);
  /// Assigns a location to the stack.
  void AssignStack(unsigned i, Type type, const Inst *value);

  /// Returns the list of GPR registers.
  llvm::ArrayRef<unsigned> GetGPRs() const;
  /// Returns the list of XMM registers.
  llvm::ArrayRef<unsigned> GetXMMs() const;

private:
  /// Number of arguments in regular registers.
  uint64_t regs_;
  /// Number of arguments in vector registers.
  uint64_t xmms_;
};
