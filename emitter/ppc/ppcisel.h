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
#include <llvm/Target/PowerPC/PPCInstrInfo.h>
#include <llvm/Target/PowerPC/PPCISelDAGToDAG.h>
#include <llvm/Target/PowerPC/PPCMachineFunctionInfo.h>
#include <llvm/Target/PowerPC/PPCRegisterInfo.h>
#include <llvm/Target/PowerPC/PPCSubtarget.h>
#include <llvm/Target/PowerPC/PPCTargetMachine.h>

#include "core/insts.h"
#include "emitter/isel.h"
#include "emitter/ppc/ppccall.h"

class Data;
class Func;
class Inst;
class Prog;

/**
 * Custom pass to generate MIR from LLIR instead of LLVM IR.
 */
class PPCISel final : public llvm::PPCDAGMatcher, public ISel {
public:
  static char ID;

  PPCISel(
      llvm::PPCTargetMachine *TM,
      llvm::PPCSubtarget *STI,
      const llvm::PPCInstrInfo *TII,
      const llvm::PPCRegisterInfo *TRI,
      const llvm::PPCTargetLowering *TLI,
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
    FuncInfo_ = MF->getInfo<llvm::PPCFunctionInfo>();
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
  /// Lowers a LL instruction.
  void LowerLL(const PPC_LLInst *inst);
  /// Lowers an SC instruction.
  void LowerSC(const PPC_SCInst *inst);
  /// Lowers a sync instruction.
  void LowerSync(const PPC_SyncInst *inst);
  /// Lowers a isync instruction.
  void LowerISync(const PPC_ISyncInst *inst);

  /// Lowers the arguments.
  void LowerArguments(bool hasVAStart) override;
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const PPCCall &ci);

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
    return PPCDAGMatcher::PreprocessISelDAG();
  }
  /// Target-specific DAG post-processing.
  void PostprocessISelDAG() override
  {
    return PPCDAGMatcher::PostprocessISelDAG();
  }

  /// Implementation of node selection.
  void Select(SDNode *node) override { return PPCDAGMatcher::Select(node); }

  /// Returns the target-specific pointer type.
  llvm::MVT GetPtrTy() const override { return llvm::MVT::i64; }
  /// Returns the target-specific condition code type.
  llvm::MVT GetFlagTy() const override;
  /// Returns the type of shift operands.
  llvm::MVT GetShiftTy() const override { return llvm::MVT::i32; }
  /// Returns the stack pointer.
  llvm::Register GetStackRegister() const override { return llvm::PPC::X1; }

private:
  /// Lowers a call target.
  std::pair<unsigned, llvm::SDValue> LowerCallee(ConstRef<Inst> inst);
  /// Lowers a call instruction.
  void LowerCallSite(llvm::SDValue chain, const CallSite *call) override;

private:
  /// Target machine.
  const llvm::PPCTargetMachine *TM_;
  /// Subtarget info.
  const llvm::PPCSubtarget *STI_;
  /// Target register info.
  const llvm::PPCRegisterInfo *TRI_;
  /// Machine function info of the current function.
  llvm::PPCFunctionInfo *FuncInfo_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
  /// Flag to indicate whether the target is a shared object.
  bool shared_;
};
