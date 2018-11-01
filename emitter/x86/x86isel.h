// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/CodeGen/MachineBasicBlock.h>
#include <llvm/CodeGen/SelectionDAG.h>
#include <llvm/CodeGen/SelectionDAGNodes.h>
#include <llvm/CodeGen/SelectionDAG/ScheduleDAGSDNodes.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Target/X86/X86Subtarget.h>
#include <llvm/Target/X86/X86TargetMachine.h>
#include <llvm/Target/X86/X86InstrInfo.h>
#include <llvm/Target/X86/X86RegisterInfo.h>
#include <llvm/Pass.h>

class Prog;
class Func;



/**
 * Custom pass to generate MIR from GenM IR instead of LLVM IR.
 */
class X86ISel final : public llvm::ModulePass {
public:
  static char ID;

  X86ISel(
      llvm::X86TargetMachine *TM,
      llvm::X86Subtarget *STI,
      llvm::X86InstrInfo *TII,
      llvm::X86RegisterInfo *TRI,
      llvm::TargetLowering *TLI,
      llvm::TargetLibraryInfo *LibInfo,
      const Prog *prog
  );

private:
  /// Creates MachineFunctions from GenM IR.
  bool runOnModule(llvm::Module &M) override;

  /// Hardcoded name.
  llvm::StringRef getPassName() const override;

  /// Requires MachineModuleInfo.
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  void CodeGenAndEmitDAG();

  void DoInstructionSelection();

  llvm::ScheduleDAGSDNodes *CreateScheduler();

private:
  /// Target machine.
  llvm::TargetMachine *TM_;
  /// Subtarget info.
  llvm::X86Subtarget *STI_;
  /// Target instruction info.
  llvm::X86InstrInfo *TII_;
  /// Target register info.
  llvm::X86RegisterInfo *TRI_;
  /// Target lowering.
  llvm::TargetLowering *TLI_;
  /// Target library info.
  llvm::TargetLibraryInfo *LibInfo_;
  /// Dummy function type.
  llvm::FunctionType *funcTy_;
  /// Program to lower.
  const Prog *prog_;
  /// Optimisation level.
  llvm::CodeGenOpt::Level opt_;
  /// Current selection DAG.
  llvm::SelectionDAG DAG_;
  /// Size of the DAG.
  unsigned DAGSize_;
  /// Dummy debug location.
  llvm::DebugLoc DL_;
  /// Dummy SelectionDAG debug location.
  llvm::SDLoc SDL_;
  /// Current machine function.
  llvm::MachineFunction *MF_;
  /// Current basic block.
  llvm::MachineBasicBlock *MBB_;
  /// Current insertion point.
  llvm::MachineBasicBlock::iterator insert_;
};
