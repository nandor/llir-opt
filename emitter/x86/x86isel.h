// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_map>

#include <llvm/ADT/DenseMap.h>
#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/CodeGen/MachineBasicBlock.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Pass.h>
#include <llvm/Target/X86/X86InstrInfo.h>
#include <llvm/Target/X86/X86ISelDAGToDAG.h>
#include <llvm/Target/X86/X86MachineFunctionInfo.h>
#include <llvm/Target/X86/X86RegisterInfo.h>
#include <llvm/Target/X86/X86Subtarget.h>
#include <llvm/Target/X86/X86TargetMachine.h>

#include "core/insts.h"
#include "emitter/isel.h"
#include "emitter/x86/x86call.h"

class Data;
class Func;
class Inst;
class Prog;



/**
 * Implementation of the DAG matcher.
 */
class X86Matcher : public llvm::X86DAGMatcher {
public:
  /// Construct a new wrapper around the LLVM selector.
  X86Matcher(
      llvm::X86TargetMachine &tm,
      llvm::CodeGenOpt::Level ol,
      llvm::MachineFunction &mf
  );
  /// Delete the matcher.
  ~X86Matcher();

  /// Return the current DAG.
  llvm::SelectionDAG &GetDAG() const { return *CurDAG; }
  /// Return the X86 target machine.
  const llvm::X86TargetMachine &getTargetMachine() const override { return tm_; }

private:
  /// X86 target machine.
  llvm::X86TargetMachine &tm_;
};

/**
 * Custom pass to generate MIR from LLIR instead of LLVM IR.
 */
class X86ISel final : public ISel {
public:
  static char ID;

  X86ISel(
      llvm::X86TargetMachine &tm,
      llvm::TargetLibraryInfo &libInfo,
      const Prog &prog,
      llvm::CodeGenOpt::Level ol,
      bool shared
  );

private:
  /// Start lowering a function.
  void Lower(llvm::MachineFunction &mf) override
  {
    m_.reset(new X86Matcher(tm_, ol_, mf));
  }

  /// Lowers a call target.
  llvm::SDValue LowerCallee(ConstRef<Inst> inst);

  /// Lowers a target-specific instruction.
  void LowerArch(const Inst *inst) override;

  /// Lowers a system call instruction.
  void LowerSyscall(const SyscallInst *inst) override;
  /// Lowers a process clone instruction.
  void LowerClone(const CloneInst *inst) override;
  /// Lowers a return.
  void LowerReturn(const ReturnInst *inst) override;
  /// Lowers an indirect jump.
  void LowerRaise(const RaiseInst *inst) override;
  /// Lowers a landing pad.
  void LowerLandingPad(const LandingPadInst *inst) override;
  /// Lowers a fixed register set instruction.
  void LowerSet(const SetInst *inst) override;

  /// Lowers an atomic exchange instruction.
  void LowerXchg(const X86_XchgInst *inst);
  /// Lowers a compare and exchange instruction.
  void LowerCmpXchg(const X86_CmpXchgInst *inst);
  /// Lowers a FnStCw instruction.
  void LowerFnClEx(const X86_FnClExInst *inst);
  /// Lowers an FPU control instruction.
  void LowerFPUControl(
      unsigned opcode,
      unsigned bytes,
      bool store,
      const Inst *inst
  );
  /// Lowers a RDTSC instruction.
  void LowerRdtsc(const X86_RdtscInst *inst);

  /// Lowers the arguments.
  void LowerArguments(bool hasVAStart) override;
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const X86Call &lowering);

private:
  /// Reads the value from %fs:0
  SDValue LoadRegArch(ConstantReg::Kind reg) override;
  /// Lowers a write to RSP.
  void LowerSetSP(SDValue value);
  /// Lowers a raise construct.
  void LowerRaise(SDValue spVal, SDValue pcVal, SDValue glue);

private:
  /// Returns the current DAG.
  llvm::SelectionDAG &GetDAG() const override { return m_->GetDAG(); }
  /// Target-specific DAG pre-processing.
  void PreprocessISelDAG() override { m_->PreprocessISelDAG(); }
  /// Target-specific DAG post-processing.
  void PostprocessISelDAG() override { m_->PostprocessISelDAG(); }
  /// Implementation of node selection.
  void Select(SDNode *node) override { m_->Select(node); }

  /// Returns the target-specific condition code type.
  llvm::MVT GetFlagTy() const override { return llvm::MVT::i8; }
  /// Returns the stack pointer.
  llvm::Register GetStackRegister() const override { return llvm::X86::RSP; }

private:
  /// Lowers a call instruction.
  void LowerCallSite(llvm::SDValue chain, const CallSite *call) override;

private:
  /// Target machine.
  llvm::X86TargetMachine &tm_;
  /// X86 matcher.
  std::unique_ptr<X86Matcher> m_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
  /// Flag to indicate whether the target is a shared object.
  bool shared_;
};
