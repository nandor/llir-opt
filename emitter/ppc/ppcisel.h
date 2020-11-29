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
 * Implementation of the DAG matcher.
 */
class PPCMatcher : public llvm::PPCDAGMatcher {
public:
  /// Construct a new wrapper around the LLVM selector.
  PPCMatcher(
      llvm::PPCTargetMachine &tm,
      llvm::CodeGenOpt::Level ol,
      llvm::MachineFunction &mf
  );
  /// Delete the matcher.
  ~PPCMatcher();

  /// Return the current DAG.
  llvm::SelectionDAG &GetDAG() const { return *CurDAG; }
  /// Returns the target lowering.
  const llvm::TargetLowering *getTargetLowering() const override
  {
    return CurDAG->getMachineFunction().getSubtarget().getTargetLowering();
  }

private:
  /// PPC target machine.
  llvm::PPCTargetMachine &tm_;
};



/**
 * Custom pass to generate MIR from LLIR instead of LLVM IR.
 */
class PPCISel final : public ISel {
public:
  static char ID;

  PPCISel(
      llvm::PPCTargetMachine &tm,
      llvm::TargetLibraryInfo &libInfo,
      const Prog &prog,
      llvm::CodeGenOpt::Level ol,
      bool shared
  );

private:
  /// Start lowering a function.
  void Lower(llvm::MachineFunction &mf) override
  {
    m_.reset(new PPCMatcher(tm_, ol_, mf));
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
  /// Lowers a TOC store instruction.
  llvm::SDValue StoreTOC(llvm::SDValue chain);

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
  llvm::MVT GetFlagTy() const override;
  /// Returns the stack pointer.
  llvm::Register GetStackRegister() const override { return llvm::PPC::X1; }

private:
  /// Returns the callee, following mov chains.
  ConstRef<Value> GetCallee(ConstRef<Inst> inst);
  /// Lowers a call target.
  std::pair<unsigned, llvm::SDValue> LowerCallee(
      ConstRef<Value> inst,
      llvm::SDValue &chain,
      llvm::SDValue &glue
  );
  /// Lowers a call instruction.
  void LowerCallSite(llvm::SDValue chain, const CallSite *call) override;

private:
  /// Target machine.
  llvm::PPCTargetMachine &tm_;
  /// PPC matcher.
  std::unique_ptr<PPCMatcher> m_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
  /// Flag to indicate whether the target is a shared object.
  bool shared_;
};
