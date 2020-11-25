// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/MC/MCRegister.h>

#include "core/inst.h"
#include "core/func.h"
#include "core/insts/call.h"
#include "emitter/call_lowering.h"

class CallInst;
class InvokeInst;
class TailCallInst;
class ReturnInst;
class RaiseInst;
class Func;



/**
 * RISCV calling convention classification.
 */
class RISCVCall final : public CallLowering {
public:
  /// Analyses a function for arguments.
  RISCVCall(const Func *func)
    : CallLowering(func)
    , numFixedArgs_(func->GetNumParams())
  {
    AnalyseFunc(func);
  }

  /// Analyses a call site.
  RISCVCall(const CallSite *inst)
    : CallLowering(inst)
    , numFixedArgs_(inst->GetNumFixedArgs().value_or(inst->arg_size()))
  {
    AnalyseCall(inst);
  }

  /// Analyses a return site.
  RISCVCall(const ReturnInst *inst)
    : CallLowering(inst)
  {
    AnalyseReturn(inst);
  }

  /// Analyses a return site.
  RISCVCall(const LandingPadInst *inst)
    : CallLowering(inst)
  {
    AnalysePad(inst);
  }

  /// Analyses a raise site.
  RISCVCall(const RaiseInst *inst)
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
  /// Number of fixed args to a call.
  unsigned numFixedArgs_ = 0;
  /// Number of arguments in integer registers.
  unsigned argI_ = 0;
  /// Number of arguments in floating-point registers.
  unsigned argF_ = 0;
  /// Number of return values in integer registers.
  unsigned retI_ = 0;
  /// Number of return values in floating-point registers.
  unsigned retF_ = 0;
  /// Number of bytes allocated on the stack.
  unsigned stack_ = 0;
};
