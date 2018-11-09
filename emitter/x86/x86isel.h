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
class ArgInst;
class AddrInst;
class CmpInst;
class LoadInst;
class StoreInst;
class ImmInst;
class UnaryInst;
class BinaryInst;
class JumpCondInst;
class ReturnInst;
class TrapInst;
enum class Type;


/**
 * Custom pass to generate MIR from GenM IR instead of LLVM IR.
 */
class X86ISel final : public llvm::X86DAGMatcher, public llvm::ModulePass {
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
  /// Lowers a data segment.
  void LowerData(const Data *data);

  /// Lowers an instruction.
  void Lower(const Inst *inst);

  /// Lowers a binary instruction.
  void LowerBinary(const Inst *inst, unsigned opcode);
  /// Lowers a conditional jump true instruction.
  void LowerJCC(const JumpCondInst *inst);
  /// Lowers a load.
  void LowerLD(const LoadInst *inst);
  /// Lowers a store.
  void LowerST(const StoreInst *inst);
  /// Lowers a return.
  void LowerReturn(const ReturnInst *inst);
  /// Lowers a call instructions.
  void LowerCall(const Inst *inst);
  /// Lowers a constant.
  void LowerImm(const ImmInst *inst);
  /// Lowers an address.
  void LowerAddr(const AddrInst *inst);
  /// Lowers an argument.
  void LowerArg(const ArgInst *inst);
  /// Lowers a comparison instruction.
  void LowerCmp(const CmpInst *inst);
  /// Lowers a trap instruction.
  void LowerTrap(const TrapInst *inst);

  /// Looks up an existing value.
  llvm::SDValue GetValue(const Inst *inst);
  /// Converts a type.
  llvm::MVT GetType(Type t);
  /// Converts a condition code.
  llvm::ISD::CondCode GetCond(Cond cc);

private:
  void CodeGenAndEmitDAG();
  void DoInstructionSelection();
  llvm::ScheduleDAGSDNodes *CreateScheduler();

private:
  /// Target register info.
  const llvm::X86RegisterInfo *TRI_;
  /// Target library info.
  llvm::TargetLibraryInfo *LibInfo_;
  /// Void pointer type.
  llvm::Type *voidTy_;
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
  /// Current module.
  llvm::Module *M;
  /// Current basic block.
  llvm::MachineBasicBlock *MBB_;
  /// Current insertion point.
  llvm::MachineBasicBlock::iterator insert_;
  /// Mapping from blocks to machine blocks.
  llvm::DenseMap<const Block *, llvm::MachineBasicBlock *> blocks_;
  /// Mapping from nodes to values.
  llvm::DenseMap<const Inst *, llvm::SDValue> values_;
  /// Chains of values in the SelectionDAG.
  llvm::SDValue Chain;
};
