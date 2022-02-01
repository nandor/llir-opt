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
      Target &target,
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
  /// Lowers a spawn instruction.
  void LowerSpawn(const SpawnInst *inst) override;
  /// Lowers a landing pad.
  void LowerLandingPad(const LandingPadInst *inst) override;
  /// Lowers a fixed register set instruction.
  void LowerSet(const SetInst *inst) override;

  /// Lowers an FPU control instruction.
  void LowerFPUControl(
      unsigned opcode,
      unsigned bytes,
      bool store,
      const Inst *inst
  );

  /// Lowers a mfence instruction.
  void Lower(const BarrierInst *inst, unsigned op);
  /// Lowers a CPUID instruction.
  void Lower(const X86_CpuIdInst *inst);
  /// Lowers an atomic exchange instruction.
  void Lower(const X86_XchgInst *inst);
  /// Lowers a compare and exchange instruction.
  void Lower(const X86_CmpXchgInst *inst);
  /// Lowers a FnStCw instruction.
  void Lower(const X86_FnClExInst *inst);
  /// Lowers a RDTSC instruction.
  void Lower(const X86_RdTscInst *inst);
  /// Lowers an IN instruction.
  void Lower(const X86_InInst *inst);
  /// Lowers an OUT instruction.
  void Lower(const X86_OutInst *inst);
  /// Lowers a WRMSR instruction.
  void Lower(const X86_WrMsrInst *inst);
  /// Lowers a RDMSR instruction.
  void Lower(const X86_RdMsrInst *inst);
  /// Lowers a PAUSE instruction.
  void Lower(const X86_PauseInst *inst);
  /// Lowers a YIELD instruction.
  void Lower(const X86_YieldInst *inst);
  /// Lowers a STI instruction.
  void Lower(const X86_StiInst *inst);
  /// Lowers a CLI instruction.
  void Lower(const X86_CliInst *inst);
  /// Lowers a HLT instruction.
  void Lower(const X86_HltInst *inst);
  /// Lowers a STI;NOP;CLI instruction sequence.
  void Lower(const X86_SpinInst *inst);
  /// Lowers a LGDT instruction.
  void Lower(const X86_LgdtInst *inst);
  /// Lowers a LIDT instruction.
  void Lower(const X86_LidtInst *inst);
  /// Lowers a LTR instruction.
  void Lower(const X86_LtrInst *inst);
  /// Lowers an xsaveopt instruction.
  void Lower(const X86_XSaveOptInst *inst);
  /// Lowers an xrestore instruction.
  void Lower(const X86_XRestoreInst *inst);

  /// Lowers the arguments.
  void LowerArguments(bool hasVAStart) override;
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const X86Call &lowering);

  /// Finalize the lowering.
  virtual bool Finalize(llvm::MachineFunction &MF) override;

private:
  /// Reads the value from %fs:0
  SDValue GetRegArch(Register reg) override;
  /// Lowers a write to RSP.
  void LowerSetSP(SDValue value);
  /// Lowers a write to a hardware register.
  void LowerSetReg(const char *code, MVT type, SDValue value);
  /// Lowers a raise construct.
  void LowerRaise(SDValue spVal, SDValue pcVal, SDValue glue);
  /// Lower a context instruction with a mask.
  void LowerContextMask(bool store, unsigned op, SDValue addr, SDValue mask);
  /// Emit a code section to set %cs.
  void LowerSetCs(SDValue value);

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
  llvm::Register GetStackRegister() const override;

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
