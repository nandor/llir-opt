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
 * Implementation of the DAG matcher.
 */
class RISCVMatcher : public llvm::RISCVDAGMatcher {
public:
  /// Construct a new wrapper around the LLVM selector.
  RISCVMatcher(
      llvm::RISCVTargetMachine &tm,
      llvm::CodeGenOpt::Level ol,
      llvm::MachineFunction &mf
  );
  /// Delete the matcher.
  ~RISCVMatcher();

  /// Return the current DAG.
  llvm::SelectionDAG &GetDAG() const { return *CurDAG; }
  /// Returns the target lowering.
  const llvm::TargetLowering *getTargetLowering() const override
  {
    return CurDAG->getMachineFunction().getSubtarget().getTargetLowering();
  }

private:
  /// RISCV target machine.
  llvm::RISCVTargetMachine &tm_;
};



/**
 * Custom pass to generate MIR from LLIR instead of LLVM IR.
 */
class RISCVISel final : public ISel {
public:
  static char ID;

  RISCVISel(
      llvm::RISCVTargetMachine &tm,
      llvm::TargetLibraryInfo &libInfo,
      const Prog &prog,
      llvm::CodeGenOpt::Level ol,
      bool shared
  );

private:
  /// Start lowering a function.
  void Lower(llvm::MachineFunction &mf) override
  {
    m_.reset(new RISCVMatcher(tm_, ol_, mf));
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
  /// Lowers an exchange instruction.
  void LowerXchg(const RISCV_XchgInst *inst);
  /// Lowers a compare-and-exchange instruction.
  void LowerCmpXchg(const RISCV_CmpXchgInst *inst);
  /// Lowers a FENCE instruction.
  void LowerFence(const RISCV_FenceInst *inst);
  /// Lowers the $gp initialisation instruction.
  void LowerGP(const RISCV_GPInst *inst);

  /// Lowers the arguments.
  void LowerArguments(bool hasVAStart) override;
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const RISCVCall &ci);

private:
  /// Returns the current DAG.
  llvm::SelectionDAG &GetDAG() const override { return m_->GetDAG(); }
  /// Target-specific DAG pre-processing.
  void PreprocessISelDAG() override { m_->PreprocessISelDAG(); }
  /// Target-specific DAG post-processing.
  void PostprocessISelDAG() override { m_->PostprocessISelDAG(); }
  /// Implementation of node selection.
  void Select(SDNode *node) override { m_->Select(node); }

  /// Returns the target-specific pointer type.
  llvm::MVT GetPtrTy() const override { return llvm::MVT::i64; }
  /// Returns the target-specific condition code type.
  llvm::MVT GetFlagTy() const override { return llvm::MVT::i32; }
  /// Returns the type of shift operands.
  llvm::MVT GetShiftTy() const override { return llvm::MVT::i64; }
  /// Returns the stack pointer.
  llvm::Register GetStackRegister() const override { return llvm::RISCV::X2; }

private:
  /// Saves vararg registers.
  void SaveVarArgRegisters(const RISCVCall &ci, bool isWin64);
  /// Lowers a call target.
  llvm::SDValue LowerCallee(ConstRef<Inst> inst);
  /// Lowers a call instruction.
  void LowerCallSite(llvm::SDValue chain, const CallSite *call) override;

private:
  /// Target machine.
  llvm::RISCVTargetMachine &tm_;
  /// RISCV matcher.
  std::unique_ptr<RISCVMatcher> m_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
  /// Flag to indicate whether the target is a shared object.
  bool shared_;
};
