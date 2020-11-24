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
#include <llvm/Target/AArch64/AArch64InstrInfo.h>
#include <llvm/Target/AArch64/AArch64ISelDAGToDAG.h>
#include <llvm/Target/AArch64/AArch64MachineFunctionInfo.h>
#include <llvm/Target/AArch64/AArch64RegisterInfo.h>
#include <llvm/Target/AArch64/AArch64Subtarget.h>
#include <llvm/Target/AArch64/AArch64TargetMachine.h>

#include "core/insts.h"
#include "emitter/isel.h"
#include "emitter/aarch64/aarch64call.h"

class Data;
class Func;
class Inst;
class Prog;

/**
 * Custom pass to generate MIR from LLIR instead of LLVM IR.
 */
class AArch64ISel final : public llvm::AArch64DAGMatcher, public ISel {
public:
  static char ID;

  AArch64ISel(
      llvm::AArch64TargetMachine *TM,
      llvm::AArch64Subtarget *STI,
      const llvm::AArch64InstrInfo *TII,
      const llvm::AArch64RegisterInfo *TRI,
      const llvm::TargetLowering *TLI,
      llvm::TargetLibraryInfo *LibInfo,
      const Prog &prog,
      llvm::CodeGenOpt::Level OL,
      bool shared
  );

private:
  /// Start lowering a function.
  void Lower(llvm::MachineFunction &mf) override
  {
    MF = &mf;
    FuncInfo_ = MF->getInfo<llvm::AArch64FunctionInfo>();
  }

  /// Reads the value from an architecture-specific register.
  SDValue LoadRegArch(ConstantReg::Kind reg) override;

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
  /// Lowers a LL instruction.
  void LowerLL(const AArch64_LL *inst);
  /// Lowers an SC instruction.
  void LowerSC(const AArch64_SC *inst);
  /// Lowers a DMB instruction.
  void LowerDMB(const AArch64_DMB *inst);

  /// Lowers the arguments.
  void LowerArguments(bool hasVAStart) override;
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const AArch64Call &ci);

private:
  /// Returns the target lowering.
  const llvm::TargetLowering *getTargetLowering() const override { return TLI; }
  /// Returns the current DAG.
  llvm::SelectionDAG &GetDAG() override { return *CurDAG; }
  /// Returns the optimisation level.
  llvm::CodeGenOpt::Level GetOptLevel() override { return OptLevel; }

  /// Returns the instruction info object.
  const llvm::TargetInstrInfo &GetInstrInfo() override { return *TII; }
  /// Returns the target lowering.
  const llvm::TargetLowering &GetTargetLowering() override { return *TLI; }
  /// Creates a new scheduler.
  llvm::ScheduleDAGSDNodes *CreateScheduler() override;
  /// Returns the register info.
  const llvm::MCRegisterInfo &GetRegisterInfo() override { return *TRI_; }

  /// Target-specific DAG pre-processing.
  void PreprocessISelDAG() override
  {
    return AArch64DAGMatcher::PreprocessISelDAG();
  }
  /// Target-specific DAG post-processing.
  void PostprocessISelDAG() override
  {
    return AArch64DAGMatcher::PostprocessISelDAG();
  }

  /// Implementation of node selection.
  void Select(SDNode *node) override { return AArch64DAGMatcher::Select(node); }

  /// Returns the target-specific pointer type.
  llvm::MVT GetPtrTy() const override { return llvm::MVT::i64; }
  /// Returns the target-specific condition code type.
  llvm::MVT GetFlagTy() const override { return llvm::MVT::i32; }
  /// Returns the type of shift operands.
  llvm::MVT GetShiftTy() const override { return llvm::MVT::i64; }
  /// Returns the stack pointer.
  llvm::Register GetStackRegister() const override { return llvm::AArch64::SP; }

private:
  /// Stores a value to sp.
  void LowerSetSP(SDValue value);
  /// Saves vararg registers.
  void SaveVarArgRegisters(const AArch64Call &ci, bool isWin64);
  /// Lowers a call target.
  llvm::SDValue LowerCallee(ConstRef<Inst> inst);
  /// Lowers a call instruction.
  void LowerCallSite(llvm::SDValue chain, const CallSite *call) override;

private:
  /// Target machine.
  const llvm::AArch64TargetMachine *TM_;
  /// Subtarget info.
  const llvm::AArch64Subtarget *STI_;
  /// Target register info.
  const llvm::AArch64RegisterInfo *TRI_;
  /// Machine function info of the current function.
  llvm::AArch64FunctionInfo *FuncInfo_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
  /// Flag to indicate whether the target is a shared object.
  bool shared_;
};
