// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/MC/MCRegister.h>

#include "core/inst.h"
#include "emitter/call_lowering.h"

class CallInst;
class InvokeInst;
class TailCallInst;
class ReturnInst;
class RaiseInst;
class Func;



/**
 * PPC calling convention classification.
 */
class PPCCall final : public CallLowering {
public:
  /// Analyses a function for arguments.
  PPCCall(const Func *func);
  /// Analyses a call site.
  PPCCall(const CallSite *inst);
  /// Analyses a return site.
  PPCCall(const ReturnInst *inst);
  /// Analyses a raise site.
  PPCCall(const RaiseInst *inst);

  /// Returns unused GPRs.
  llvm::ArrayRef<llvm::MCPhysReg> GetUnusedGPRs() const;
  /// Returns the used GPRs.
  llvm::ArrayRef<llvm::MCPhysReg> GetUsedGPRs() const;
  /// Returns unused FPRs.
  llvm::ArrayRef<llvm::MCPhysReg> GetUnusedFPRs() const;
  /// Returns the used FPRs.
  llvm::ArrayRef<llvm::MCPhysReg> GetUsedFPRs() const;
  /// Returns the number of bytes allocated on the stack.
  unsigned GetFrameSize() const override { return stack_; }

private:
  /// Location assignment for C calls.
  void AssignArgC(unsigned i, Type type, ConstRef<Inst> value) override;
  /// Location assignment for Ocaml calls.
  void AssignArgOCaml(unsigned i, Type type, ConstRef<Inst> value) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignArgOCamlAlloc(unsigned i, Type type, ConstRef<Inst> value) override
  {
    return AssignArgOCaml(i, type, value);
  }
  /// Location assignment for OCaml to GC trampolines.
  void AssignArgOCamlGc(unsigned i, Type type, ConstRef<Inst> value) override
  {
    return AssignArgOCaml(i, type, value);
  }

  /// Location assignment for C calls.
  void AssignRetC(unsigned i, Type type) override;
  /// Location assignment for Ocaml calls.
  void AssignRetOCaml(unsigned i, Type type) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignRetOCamlAlloc(unsigned i, Type type) override
  {
    return AssignRetOCaml(i, type);
  }
  /// Location assignment for OCaml to GC trampolines.
  void AssignRetOCamlGc(unsigned i, Type type) override
  {
    return AssignRetOCaml(i, type);
  }

  /// Assigns a location to a register.
  void AssignArgReg(unsigned i, llvm::MVT vt, llvm::Register reg);
  /// Assigns a location to a register.
  void AssignRetReg(unsigned i, llvm::MVT vt, llvm::Register reg);
  /// Assigns a location to the stack.
  void AssignArgStack(unsigned i, llvm::MVT type, unsigned size);

  /// Returns the list of GPR registers.
  llvm::ArrayRef<llvm::MCPhysReg> GetGPRs() const;
  /// Returns the list of FP registers.
  llvm::ArrayRef<llvm::MCPhysReg> GetFPRs() const;

  /// Check whether any arguments were stored on stack.
  bool HasStackArgs() const { return hasStackArgs_; }

private:
  /// Number of arguments in integer registers.
  uint64_t argG_ = 0;
  /// Number of return values in integer registers.
  uint64_t retG_ = 0;
  /// Number of arguments in floating-point registers.
  uint64_t argF_ = 0;
  /// Number of return values in floating-point registers.
  uint64_t retF_ = 0;
  /// Stack offset for arguments.
  unsigned stack_ = 4 * 8;
  /// Flag to indicate whether any parameters are saved on stack.
  bool hasStackArgs_ = false;
};
