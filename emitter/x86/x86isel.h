// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/DenseMap.h>
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
#include <llvm/Target/X86/X86ISelDAGToDAG.h>
#include <llvm/Pass.h>

class Prog;
class Func;
class Inst;
class UnaryInst;
class BinaryInst;



/**
 * Custom pass to generate MIR from GenM IR instead of LLVM IR.
 */
class X86ISel final : public llvm::X86DAGMatcher, public llvm::ModulePass {
public:
  static char ID;

  X86ISel(
      llvm::X86TargetMachine *TM,
      llvm::X86Subtarget *STI,
      llvm::X86InstrInfo *TII,
      llvm::X86RegisterInfo *TRI,
      llvm::TargetLowering *TLI,
      llvm::TargetLibraryInfo *LibInfo,
      const Prog *prog,
      llvm::CodeGenOpt::Level OL
  );

private:
  /// Creates MachineFunctions from GenM IR.
  bool runOnModule(llvm::Module &M) override;
  /// Hardcoded name.
  llvm::StringRef getPassName() const override;
  /// Requires MachineModuleInfo.
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
  /// Lowers an instruction.
  void Lower(const Inst *inst);

  /// Lowers a binary instruction.
  void LowerBinary(const Inst *inst, unsigned opcode);
  /// Lowers a conditional jump instruction.
  void LowerCondJump(const Inst *inst, bool when);
  /// Lowers a load.
  void LowerLoad(const Inst *inst);
  /// Lowers a store.
  void LowerStore(const Inst *inst);
  /// Lowers a return.
  void LowerReturn(const Inst *inst);
  /// Lowers a call instructions.
  void LowerCall(const Inst *inst);
  /// Lowers a constant.
  void LowerImm(const Inst *inst);
  /// Lowers an address.
  void LowerAddr(const Inst *inst);
  /// Lowers an argument.
  void LowerArg(const Inst *inst);
  /// Lowers a comparison instruction.
  void LowerCmp(const Inst *inst);

private:
  void CodeGenAndEmitDAG();
  void DoInstructionSelection();
  llvm::ScheduleDAGSDNodes *CreateScheduler();

private:
  /// Target register info.
  llvm::X86RegisterInfo *TRI_;
  /// Target library info.
  llvm::TargetLibraryInfo *LibInfo_;
  /// Dummy function type.
  llvm::FunctionType *funcTy_;
  /// Program to lower.
  const Prog *prog_;
  /// Size of the DAG.
  unsigned DAGSize_;
  /// Dummy debug location.
  llvm::DebugLoc DL_;
  /// Dummy SelectionDAG debug location.
  llvm::SDLoc SDL_;
  /// Current basic block.
  llvm::MachineBasicBlock *MBB_;
  /// Current insertion point.
  llvm::MachineBasicBlock::iterator insert_;
  /// Mapping from nodes to values.
  llvm::DenseMap<const Inst *, llvm::SDValue> values_;
};
