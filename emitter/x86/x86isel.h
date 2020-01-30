// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
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
#include "core/analysis/live_variables.h"
#include "emitter/isel.h"
#include "emitter/x86/x86call.h"

class Data;
class Func;
class Inst;
class Prog;



/**
 * Custom pass to generate MIR from GenM IR instead of LLVM IR.
 */
class X86ISel final
    : public llvm::X86DAGMatcher
    , public llvm::ModulePass
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
  /// Lowers block references.
  void LowerRefs(const Data *data);

  /// Lowers an instruction.
  void Lower(const Inst *inst);

  /// Lowers a call instructions.
  void LowerCall(const CallInst *inst);
  /// Lowers a tail call instruction.
  void LowerTailCall(const TailCallInst *inst);
  /// Lowers an invoke instruction.
  void LowerInvoke(const InvokeInst *inst);
  /// Lowers a tail invoke instruction.
  void LowerTailInvoke(const TailInvokeInst *inst);
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
  /// Lowers an alloca instruction.
  void LowerAlloca(const AllocaInst *inst);
  /// Lowers a select instruction.
  void LowerSelect(const SelectInst *inst);
  /// Lowers a vararg frame setup instruction.
  void LowerVAStart(const VAStartInst *inst);
  /// Lowers an undefined instruction.
  void LowerUndef(const UndefInst *inst);
  /// Lowers an overflow check instruction.
  void LowerALUO(const OverflowInst *inst, unsigned op);
  /// Lowers a RDTSC instruction.
  void LowerRDTSC(const RdtscInst *inst);

  /// Handle PHI nodes in successor blocks.
  void HandleSuccessorPHI(const Block *block);

  /// Lowers an argument.
  void LowerArg(const Func &func, X86Call::Loc &argLoc);
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const Func &func, X86Call &ci);

  /// Lovers a register value.
  llvm::SDValue LoadReg(const MovInst *inst, ConstantReg::Kind reg);

  /// Exports a value.
  void Export(const Inst *inst, llvm::SDValue val);

  /// Creates a register for an instruction's result.
  unsigned AssignVReg(const Inst *inst);

  /// Looks up an existing value.
  llvm::SDValue GetValue(const Inst *inst);
  /// Returns a constant if the instruction introduces one.
  llvm::SDValue GetConstant(const Inst *inst);
  /// Converts a type.
  llvm::MVT GetType(Type t);
  /// Converts a condition code.
  llvm::ISD::CondCode GetCond(Cond cc);
  /// Lowers a global value.
  llvm::SDValue LowerGlobal(const Global *val, int64_t offset);
  /// Lowers an expression value.
  llvm::SDValue LowerExpr(const Expr *expr);

private:
  /// Flushes pending exports.
  llvm::SDValue GetExportRoot();

  /// Copies a value to a vreg to be exported later.
  void CopyToVreg(unsigned reg, llvm::SDValue value);

  /// Prepares the dag for instruction selection.
  void CodeGenAndEmitDAG();

  /// Creates a MachineBasicBlock with MachineInstrs.
  void DoInstructionSelection();

  /// Fixes the ordering of annotation labels.
  void BundleAnnotations(const Block *block, llvm::MachineBasicBlock *MBB);

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
  llvm::SDValue LowerImm(const APSInt &val, Type type);
  /// Lowers an immediate to a SDValue.
  llvm::SDValue LowerImm(const APFloat &val, Type type);
  /// Lowers a call instruction.
  template<typename T> void LowerCallSite(
      llvm::SDValue chain,
      const CallSite<T> *call
  );
  /// Get the relevant vars for a GC frame.
  std::vector<std::pair<const Inst *, llvm::SDValue>> GetFrameExport(
      const Inst *frame
  );
  /// Breaks a variable.
  llvm::SDValue BreakVar(
      llvm::SDValue chain,
      const Inst *inst,
      llvm::SDValue value
  );

private:
  /// Target register info.
  const llvm::X86RegisterInfo *TRI_;
  /// Machine function info of the current function.
  llvm::X86MachineFunctionInfo *FuncInfo_;
  /// Target library info.
  llvm::TargetLibraryInfo *LibInfo_;
  /// Void type.
  llvm::Type *voidTy_;
  /// Void pointer type.
  llvm::Type *i8PtrTy_;
  /// Dummy function type.
  llvm::FunctionType *funcTy_;
  /// Type of flags.
  llvm::MVT flagTy_ = llvm::MVT::i8;
  /// Program to lower.
  const Prog *prog_;
  /// Current function.
  const Func *func_;
  /// Argument to register mapping.
  std::unique_ptr<X86Call> conv_;
  /// Argument frame indices.
  llvm::DenseMap<unsigned, int> args_;
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
  /// Mapping from nodes to values.
  llvm::DenseMap<const Inst *, llvm::SDValue> values_;
  /// Mapping from nodes to registers.
  llvm::DenseMap<const Inst *, unsigned> regs_;
  /// Mapping from stack_object indices to llvm stack objects.
  llvm::DenseMap<unsigned, unsigned> stackIndices_;
  /// Current LLVM function.
  llvm::Function *F;
  /// Variables live on exit - used to implement sets of regs.
  std::set<unsigned> liveOnExit_;
  /// Pending exports.
  std::map<unsigned, llvm::SDValue> pendingExports_;
  /// Per-function live variable info.
  std::unique_ptr<LiveVariables> lva_;
  /// Frame start index, if necessary.
  int frameIndex_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
};
