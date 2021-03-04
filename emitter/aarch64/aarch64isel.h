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
 * Implementation of the DAG matcher.
 */
class AArch64Matcher : public llvm::AArch64DAGMatcher {
public:
  /// Construct a new wrapper around the LLVM selector.
  AArch64Matcher(
      llvm::AArch64TargetMachine &tm,
      llvm::CodeGenOpt::Level ol,
      llvm::MachineFunction &mf
  );
  /// Delete the matcher.
  ~AArch64Matcher();

  /// Return the current DAG.
  llvm::SelectionDAG &GetDAG() const { return *CurDAG; }
  /// Returns the target lowering.
  const llvm::TargetLowering *getTargetLowering() const override
  {
    return CurDAG->getMachineFunction().getSubtarget().getTargetLowering();
  }

private:
  /// AArch64 target machine.
  llvm::AArch64TargetMachine &tm_;
};



/**
 * Custom pass to generate MIR from LLIR instead of LLVM IR.
 */
class AArch64ISel final : public ISel {
public:
  static char ID;

  AArch64ISel(
      llvm::AArch64TargetMachine &tm,
      llvm::TargetLibraryInfo &libInfo,
      const Prog &prog,
      llvm::CodeGenOpt::Level ol,
      bool shared
  );

private:
  /// Start lowering a function.
  void Lower(llvm::MachineFunction &mf) override
  {
    m_.reset(new AArch64Matcher(tm_, ol_, mf));
  }

  /// Reads the value from an architecture-specific register.
  SDValue GetRegArch(Register reg) override;

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
  /// Lowers a spawn instruction.
  void LowerSpawn(const SpawnInst *inst) override;
  /// Lowers a landing pad.
  void LowerLandingPad(const LandingPadInst *inst) override;
  /// Lowers a fixed register set instruction.
  void LowerSet(const SetInst *inst) override;
  /// Lowers a LL instruction.
  void LowerLoadLink(const AArch64_LoadLinkInst *inst);
  /// Lowers an SC instruction.
  void LowerStoreCond(const AArch64_StoreCondInst *inst);
  /// Lowers a DMB instruction.
  void LowerDFence(const AArch64_DFenceInst *inst);

  /// Lowers the arguments.
  void LowerArguments(bool hasVAStart) override;
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const AArch64Call &ci);

private:
  /// Returns the current DAG.
  llvm::SelectionDAG &GetDAG() const override { return m_->GetDAG(); }
  /// Target-specific DAG pre-processing.
  void PreprocessISelDAG() override { m_->PreprocessISelDAG(); }
  /// Target-specific DAG post-processing.
  void PostprocessISelDAG() override { m_->PostprocessISelDAG(); }
  /// Implementation of node selection.
  void Select(SDNode *node) override { m_->Select(node); }

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
  llvm::AArch64TargetMachine &tm_;
  /// AArch64 matcher.
  std::unique_ptr<AArch64Matcher> m_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
  /// Flag to indicate whether the target is a shared object.
  bool shared_;
};
