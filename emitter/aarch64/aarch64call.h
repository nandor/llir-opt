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
class ReturnInst;
class RaiseInst;
class Func;



/**
 * AArch64 calling convention classification.
 */
class AArch64Call final : public CallLowering {
public:
  /// Analyses a function for arguments.
  AArch64Call(const Func *func)
    : CallLowering(func)
  {
    AnalyseFunc(func);
  }

  /// Analyses a call site.
  AArch64Call(const CallSite *inst)
    : CallLowering(inst)
  {
    AnalyseCall(inst);
  }

  /// Analyses a return site.
  AArch64Call(const ReturnInst *inst)
    : CallLowering(inst)
  {
    AnalyseReturn(inst);
  }

  /// Analyses a return site.
  AArch64Call(const LandingPadInst *inst)
    : CallLowering(inst)
  {
    AnalysePad(inst);
  }

  /// Analyses a raise site.
  AArch64Call(const RaiseInst *inst)
    : CallLowering(inst)
  {
    AnalyseRaise(inst);
  }

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
  void AssignArgC(unsigned i, FlaggedType type) override;
  /// Location assignment for Ocaml calls.
  void AssignArgOCaml(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignArgOCamlAlloc(unsigned i, FlaggedType type) override;
  /// Location assignment for OCaml to GC trampolines.
  void AssignArgOCamlGc(unsigned i, FlaggedType type) override;
  /// Location assignment for Xen hypercalls.
  void AssignArgXen(unsigned i, FlaggedType type) override;
  /// Location assignment for Win64 calls.
  void AssignArgWin64(unsigned i, FlaggedType type) override;

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
  /// Location assignment for Win64 calls
  void AssignRetWin64(unsigned i, FlaggedType type) override;

  /// Assigns a location to a register.
  void AssignArgReg(ArgLoc &loc, llvm::MVT vt, llvm::Register reg);
  /// Assigns a location to the stack.
  void AssignArgStack(ArgLoc &loc, llvm::MVT type, unsigned size);
  /// Assigns a location to a register.
  void AssignRetReg(RetLoc &loc, llvm::MVT vt, llvm::Register reg);

  /// Returns the list of GPR registers.
  llvm::ArrayRef<llvm::MCPhysReg> GetGPRs() const;
  /// Returns the list of FP registers.
  llvm::ArrayRef<llvm::MCPhysReg> GetFPRs() const;

private:
  /// Number of arguments in integer registers.
  unsigned argX_ = 0;
  /// Number of arguments in floating-point registers.
  unsigned argD_ = 0;
  /// Number of return values in integer registers.
  unsigned retX_ = 0;
  /// Number of return values in floating-point registers.
  unsigned retD_ = 0;
  /// Number of bytes allocated on the stack.
  unsigned stack_ = 0;
};
