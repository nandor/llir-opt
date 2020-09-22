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
 * Custom pass to generate MIR from LLIR instead of LLVM IR.
 */
class X86ISel final
    : public llvm::X86DAGMatcher
    , public ISel
{
public:
  static char ID;

  X86ISel(
      llvm::X86TargetMachine *TM,
      llvm::X86Subtarget *STI,
      const llvm::X86InstrInfo *TII,
      const llvm::X86RegisterInfo *TRI,
      const llvm::TargetLowering *TLI,
      llvm::TargetLibraryInfo *LibInfo,
      const Prog *prog,
      llvm::CodeGenOpt::Level OL,
      bool shared
  );

private:
  /// Start lowering a function.
  void Lower(llvm::MachineFunction &mf) override
  {
    MF = &mf;
    FuncInfo_ = MF->getInfo<llvm::X86MachineFunctionInfo>();
  }

  /// Lowers a refrence to a global.
  llvm::SDValue LowerGlobal(const Global *val, int64_t offset) override;
  /// Lovers a register value.
  llvm::SDValue LoadReg(ConstantReg::Kind reg) override;
  /// Returns the call lowering for the current function.
  CallLowering &GetCallLowering() override { return GetX86CallLowering(); }

  /// Lowers a call instructions.
  void LowerCall(const CallInst *inst) override;
  /// Lowers a tail call instruction.
  void LowerTailCall(const TailCallInst *inst) override;
  /// Lowers an invoke instruction.
  void LowerInvoke(const InvokeInst *inst) override;
  /// Lowers a tail invoke instruction.
  void LowerTailInvoke(const TailInvokeInst *inst) override;
  /// Lowers a system call instruction.
  void LowerSyscall(const SyscallInst *inst) override;
  /// Lowers a return.
  void LowerReturn(const ReturnInst *inst) override;
  /// Lowers a compare and exchange instructions.
  void LowerCmpXchg(const CmpXchgInst *inst) override;
  /// Lowers a RDTSC instruction.
  void LowerRDTSC(const RdtscInst *inst) override;
  /// Lowers a FNStCw instruction.
  void LowerFNStCw(const FNStCwInst *inst) override;
  /// Lowers a FLdCw instruction.
  void LowerFLdCw(const FLdCwInst *inst) override;
  /// Lowers a vararg frame setup instruction.
  void LowerVAStart(const VAStartInst *inst) override;
  /// Lowers a switch.
  void LowerSwitch(const SwitchInst *inst) override;
  /// Lowers a fixed register set instruction.
  void LowerSet(const SetInst *inst) override;

  /// Lowers all arguments.
  void LowerArgs() override;
  /// Lowers variable argument list frame setup.
  void LowerVASetup() override;

private:
  /// Returns the X86 target machine.
  const llvm::X86TargetMachine &getTargetMachine() const override
  {
    return *TM_;
  }

  /// Returns the current DAG.
  llvm::SelectionDAG &GetDAG() override { return *CurDAG; }
  /// Returns the optimisation level.
  llvm::CodeGenOpt::Level GetOptLevel() override { return OptLevel; }

  /// Returns the register info object.
  llvm::MachineRegisterInfo &GetRegisterInfo() override
  {
    return MF->getRegInfo();
  }

  /// Returns the instruction info object.
  const llvm::TargetInstrInfo &GetInstrInfo() override { return *TII; }
  /// Returns the target lowering.
  const llvm::TargetLowering &GetTargetLowering() override { return *TLI; }
  /// Creates a new scheduler.
  llvm::ScheduleDAGSDNodes *CreateScheduler() override;

  /// Target-specific DAG preprocessing.
  void PreprocessISelDAG() override
  {
    return X86DAGMatcher::PreprocessISelDAG();
  }

  /// Implementation of node selection.
  void Select(SDNode *node) override { return X86DAGMatcher::Select(node); }

  /// Returns the target-specific pointer type.
  llvm::MVT GetPtrTy() const override { return llvm::MVT::i64; }
  /// Returns the target-specific condition code type.
  llvm::MVT GetFlagTy() const override { return llvm::MVT::i8; }

private:
  /// Lowers a call instruction.
  template<typename T> void LowerCallSite(
      llvm::SDValue chain,
      const CallSite<T> *call
  );
  /// Breaks a variable.
  llvm::SDValue BreakVar(
      llvm::SDValue chain,
      const Inst *inst,
      llvm::SDValue value
  );

  /// Returns the X86-specific calling conv object.
  X86Call &GetX86CallLowering();

private:
  /// Target machine.
  const llvm::X86TargetMachine *TM_;
  /// Target register info.
  const llvm::X86RegisterInfo *TRI_;
  /// Machine function info of the current function.
  llvm::X86MachineFunctionInfo *FuncInfo_;
  /// Argument to register mapping.
  std::unique_ptr<std::pair<const Func *, X86Call>> conv_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
  /// Flag to indicate whether the target is a shared object.
  bool shared_;
};
