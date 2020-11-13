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
#include <llvm/Target/RISCV/RISCVInstrInfo.h>
#include <llvm/Target/RISCV/RISCVISelDAGToDAG.h>
#include <llvm/Target/RISCV/RISCVMachineFunctionInfo.h>
#include <llvm/Target/RISCV/RISCVRegisterInfo.h>
#include <llvm/Target/RISCV/RISCVSubtarget.h>
#include <llvm/Target/RISCV/RISCVTargetMachine.h>

#include "core/insts.h"
#include "emitter/isel.h"
#include "emitter/riscv/riscvcall.h"

class Data;
class Func;
class Inst;
class Prog;

/**
 * Custom pass to generate MIR from LLIR instead of LLVM IR.
 */
class RISCVISel final : public llvm::RISCVDAGMatcher, public ISel {
public:
  static char ID;

  RISCVISel(
      llvm::RISCVTargetMachine *TM,
      llvm::RISCVSubtarget *STI,
      const llvm::RISCVInstrInfo *TII,
      const llvm::RISCVRegisterInfo *TRI,
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
    FuncInfo_ = MF->getInfo<llvm::RISCVMachineFunctionInfo>();
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
  /// Lowers a fixed register set instruction.
  void LowerSet(const SetInst *inst) override;

  /// Lowers the arguments.
  void LowerArguments(bool hasVAStart) override;
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const RISCVCall &ci);

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
    return RISCVDAGMatcher::PreprocessISelDAG();
  }
  /// Target-specific DAG post-processing.
  void PostprocessISelDAG() override
  {
    return RISCVDAGMatcher::PostprocessISelDAG();
  }

  /// Implementation of node selection.
  void Select(SDNode *node) override { return RISCVDAGMatcher::Select(node); }

  /// Returns the target-specific pointer type.
  llvm::MVT GetPtrTy() const override { return llvm::MVT::i64; }
  /// Returns the target-specific condition code type.
  llvm::MVT GetFlagTy() const override { return llvm::MVT::i32; }
  /// Returns the stack pointer.
  llvm::Register GetStackRegister() const override { return llvm::RISCV::X2; }

private:
  /// Stores a value to sp.
  void LowerSetSP(SDValue value);
  /// Saves vararg registers.
  void SaveVarArgRegisters(const RISCVCall &ci, bool isWin64);
  /// Lowers a call target.
  llvm::SDValue LowerCallee(ConstRef<Inst> inst);
  /// Lowers a call instruction.
  void LowerCallSite(llvm::SDValue chain, const CallSite *call) override;

private:
  /// Target machine.
  const llvm::RISCVTargetMachine *TM_;
  /// Subtarget info.
  const llvm::RISCVSubtarget *STI_;
  /// Target register info.
  const llvm::RISCVRegisterInfo *TRI_;
  /// Machine function info of the current function.
  llvm::RISCVMachineFunctionInfo *FuncInfo_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
  /// Flag to indicate whether the target is a shared object.
  bool shared_;
};
