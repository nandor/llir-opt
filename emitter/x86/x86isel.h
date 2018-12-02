// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include <llvm/ADT/DenseMap.h>
#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/CodeGen/MachineBasicBlock.h>
#include <llvm/CodeGen/SelectionDAG.h>
#include <llvm/CodeGen/SelectionDAG/ScheduleDAGSDNodes.h>
#include <llvm/CodeGen/SelectionDAGNodes.h>
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
#include "emitter/x86/x86call.h"

class Data;
class Func;
class Inst;
class Prog;



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

  /// Finds the MachineFunction attached to a function.
  llvm::MachineFunction *operator[] (const Func *func) const;
  /// Finds the label attached to an instruction.
  llvm::MCSymbol *operator[] (const Inst *inst) const;
  /// Finds the MachineBasicBlock attached to a block.
  llvm::MachineBasicBlock *operator[] (const Block *inst) const;

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

  /// Lowers a call instructions.
  void LowerCall(const CallInst *inst);
  /// Lowers a tail call instruction.
  void LowerTailCall(const TailCallInst *inst);
  /// Lowers an invoke instruction.
  void LowerInvoke(const InvokeInst *inst);
  /// Lowers a binary instruction.
  void LowerBinary(const Inst *inst, unsigned op);
  /// Lowers a binary integer or float operation.
  void LowerBinary(const Inst *inst, unsigned uop, unsigned sop, unsigned fop);
  /// Lowers a unary instruction.
  void LowerUnary(const UnaryInst *inst, unsigned opcode);
  /// Lowers a conditional jump true instruction.
  void LowerJCC(const JumpCondInst *inst);
  /// Lowers an indirect jump.
  void LowerJI(const JumpIndirectInst *inst);
  /// Lowers a jump instruction.
  void LowerJMP(const JumpInst *inst);
  /// Lowers a switch.
  void LowerSwitch(const SwitchInst *inst);
  /// Lowers a load.
  void LowerLD(const LoadInst *inst);
  /// Lowers a store.
  void LowerST(const StoreInst *inst);
  /// Lowers a return.
  void LowerReturn(const ReturnInst *inst);
  /// Lowers a frame instruction.
  void LowerFrame(const FrameInst *inst);
  /// Lowers a comparison instruction.
  void LowerCmp(const CmpInst *inst);
  /// Lowers a trap instruction.
  void LowerTrap(const TrapInst *inst);
  /// Lowers a mov instruction.
  void LowerMov(const MovInst *inst);
  /// Lowers a sign extend instruction.
  void LowerSExt(const SExtInst *inst);
  /// Lowers a zero extend instruction.
  void LowerZExt(const ZExtInst *inst);
  /// Lowers a float extend instruction.
  void LowerFExt(const FExtInst *inst);
  /// Lowers a truncate instruction.
  void LowerTrunc(const TruncInst *inst);
  /// Lowers an exchange instruction.
  void LowerXCHG(const ExchangeInst *inst);
  /// Lowers a fixed register set instruction.
  void LowerSet(const SetInst *inst);
  /// Lowers a select instruction.
  void LowerSelect(const SelectInst *inst);
  /// Lowers a vararg frame setup instruction.
  void LowerVAStart(const VAStartInst *inst);
  /// Lowers an undefined instruction.
  void LowerUndef(const UndefInst *inst);

  /// Handle PHI nodes in successor blocks.
  void HandleSuccessorPHI(const Block *block);

  /// Lowers an argument.
  void LowerArg(const Func &func, X86Call::Loc &argLoc);
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const Func &func, X86Call &ci);

  /// Exports a value.
  void Export(const Inst *inst, llvm::SDValue val);

  /// Creates a register for an instruction's result.
  unsigned AssignVReg(const Inst *inst);

  /// Looks up an existing value.
  llvm::SDValue GetValue(const Inst *inst);
  /// Converts a type.
  llvm::MVT GetType(Type t);
  /// Converts a condition code.
  llvm::ISD::CondCode GetCond(Cond cc);

private:
  /// Prepares the dag for instruction selection.
  void CodeGenAndEmitDAG();

  /// Creates a MachineBasicBlock with MachineInstrs.
  void DoInstructionSelection();

  /// Fixes the ordering of annotation labels.
  void ScheduleAnnotations(const Block *block, llvm::MachineBasicBlock *MBB);

  /// Creates a machine instruction selection.
  llvm::ScheduleDAGSDNodes *CreateScheduler();

  /// Wrapper union to bitcast between types.
  union ImmValue {
    float f32v;
    double f64v;
    int8_t i8v;
    int16_t i16v;
    int32_t i32v;
    int64_t i64v;

    ImmValue(int64_t v) : i64v(v) {}
    ImmValue(double v) : f64v(v) {}
  };

  /// Lowers an immediate to a SDValue.
  llvm::SDValue LowerImm(ImmValue val, Type type);
  /// Lowers a call instruction.
  template<typename T> void LowerCallSite(const CallSite<T> *call);

private:
  /// Target register info.
  const llvm::X86RegisterInfo *TRI_;
  /// Machine function info of the current function.
  llvm::X86MachineFunctionInfo *FuncInfo_;
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
  /// Mapping from functions to MachineFunctions.
  std::unordered_map<const Func *, llvm::MachineFunction *> funcs_;
  /// Mapping from blocks to machine blocks.
  llvm::DenseMap<const Block *, llvm::MachineBasicBlock *> blocks_;
  /// Labels of annotated instructions.
  std::unordered_map<const Inst *, llvm::MCSymbol *> labels_;
  /// Mapping from nodes to values.
  llvm::DenseMap<const Inst *, llvm::SDValue> values_;
  /// Mapping from nodes to registers.
  llvm::DenseMap<const Inst *, unsigned> regs_;
  /// Chains of values in the SelectionDAG.
  llvm::SDValue Chain;
  /// Current LLVM function.
  llvm::Function *F;
  /// Current stack frame index.
  unsigned stackIndex_;
};
