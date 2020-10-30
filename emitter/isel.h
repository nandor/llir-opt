// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/IR/CallingConv.h>
#include <llvm/CodeGen/MachineRegisterInfo.h>
#include <llvm/CodeGen/SelectionDAG.h>
#include <llvm/CodeGen/SelectionDAG/ScheduleDAGSDNodes.h>
#include <llvm/CodeGen/SelectionDAGNodes.h>

#include "core/analysis/live_variables.h"
#include "core/insts.h"
#include "core/type.h"
#include "emitter/isel_mapping.h"
#include "emitter/call_lowering.h"



/**
 * Base class for instruction selectors.
 */
class ISel : public llvm::ModulePass, public ISelMapping {
protected:
  using MVT = llvm::MVT;
  using EVT = llvm::EVT;
  using SDNode = llvm::SDNode;
  using SDValue = llvm::SDValue;
  using SDVTList = llvm::SDVTList;
  using SelectionDAG = llvm::SelectionDAG;
  using GlobalValue = llvm::GlobalValue;

protected:
  /// Initialises the instruction selector.
  ISel(char &ID, const Prog *prog, llvm::TargetLibraryInfo *libInfo);

private:
  /// Return the name of the pass.
  llvm::StringRef getPassName() const override;
  /// Requires MachineModuleInfo.
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  /// Creates MachineFunctions from LLIR.
  bool runOnModule(llvm::Module &M) override;

protected:
  /// Lowers an instruction.
  void Lower(const Inst *inst);

protected:
  /// Start lowering a function.
  virtual void Lower(llvm::MachineFunction &mf) = 0;

  /// Reads the value from %fs:0
  virtual SDValue LowerGetFS() = 0;

  /// Lowers a system call instruction.
  virtual void LowerSyscall(const SyscallInst *inst) = 0;
  /// Lowers a process clone instruction.
  virtual void LowerClone(const CloneInst *inst) = 0;
  /// Lowers a return.
  virtual void LowerReturn(const ReturnInst *inst) = 0;
  /// Lowers a vararg frame setup instruction.
  virtual void LowerVAStart(const VAStartInst *inst) = 0;
  /// Lowers an indirect jump.
  virtual void LowerRaise(const RaiseInst *inst) = 0;
  /// Lowers a fixed register set instruction.
  virtual void LowerSet(const SetInst *inst) = 0;
  /// Lowers a target-specific instruction.
  virtual void LowerArch(const Inst *inst) = 0;

  /// Lowers variable argument list frame setup.
  virtual void LowerArguments(bool hasVAStart) = 0;

  /// Returns the optimisation level.
  virtual llvm::CodeGenOpt::Level GetOptLevel() = 0;
  /// Returns a reference to the current DAG.
  virtual llvm::SelectionDAG &GetDAG() = 0;
  /// Returns the instr info.
  virtual const llvm::TargetInstrInfo &GetInstrInfo() = 0;
  /// Returns the target lowering info.
  virtual const llvm::TargetLowering &GetTargetLowering() = 0;
  /// Returns the register info.
  virtual const llvm::MCRegisterInfo &GetRegisterInfo() = 0;

  /// Returns the target pointer type.
  virtual llvm::MVT GetPtrTy() const = 0;
  /// Returns the type of condition code flags.
  virtual llvm::MVT GetFlagTy() const = 0;

  /// Creates a machine instruction selector.
  virtual llvm::ScheduleDAGSDNodes *CreateScheduler() = 0;
  /// Target-specific preprocessing step.
  virtual void PreprocessISelDAG() = 0;
  /// Target-specific post-processing step.
  virtual void PostprocessISelDAG() = 0;
  /// Target-specific instruction selection.
  virtual void Select(SDNode *node) = 0;

protected:
  /// Lovers a register value.
  SDValue LoadReg(ConstantReg::Kind reg);
  /// Lowers an offset reference to a global.
  llvm::SDValue LowerGlobal(const Global &val);
  /// Lowers a global value.
  llvm::SDValue LowerGlobal(const Global &val, int64_t offset);
  /// Lowers all arguments.
  void LowerArgs(CallLowering &lowering);

  using ExportList = std::vector<std::pair<unsigned, SDValue>>;
  /// Flushes pending exports that are not OCaml values.
  SDValue GetPrimitiveExportRoot();
  /// Flushes pending exports which are OCaml values.
  SDValue GetValueExportRoot();
  /// Flushes all pending exports.
  SDValue GetExportRoot();
  /// Export a set of values.
  SDValue GetExportRoot(const ExportList &exports);
  /// Checks if there are any pending exports.
  bool HasPendingExports();

  /// Copies a value to a vreg to be exported later.
  void ExportValue(unsigned reg, SDValue value);
  /// Creates a register for an instruction's result.
  unsigned AssignVReg(ConstRef<Inst> inst);

  /// Lower an inline asm sequence.
  SDValue LowerInlineAsm(
      const char *code,
      unsigned flags,
      llvm::ArrayRef<llvm::Register> inputs,
      llvm::ArrayRef<llvm::Register> clobbers,
      llvm::ArrayRef<llvm::Register> outputs,
      SDValue glue = SDValue()
  );

  /// Lowers an immediate to a SDValue.
  SDValue LowerImm(const APInt &val, Type type);
  /// Lowers an immediate to a SDValue.
  SDValue LowerImm(const APFloat &val, Type type);
  /// Returns a constant if the instruction introduces one.
  SDValue LowerConstant(ConstRef<Inst> inst);
  /// Lowers an expression value.
  SDValue LowerExpr(const Expr &expr);

  /// Looks up an existing value.
  SDValue GetValue(ConstRef<Inst> inst);
  /// Exports a value.
  void Export(ConstRef<Inst> inst, llvm::SDValue val);

  /// Converts a condition code.
  llvm::ISD::CondCode GetCond(Cond cc);

  /// Vector of exported values from the frame.
  using FrameExports = std::vector<std::pair<ConstRef<Inst>, llvm::SDValue>>;
  /// Get the relevant vars for a GC frame.
  FrameExports GetFrameExport(const Inst *frame);
  /// Lower a GC frame.
  llvm::SDValue LowerGCFrame(
      llvm::SDValue chain,
      llvm::SDValue glue,
      const Inst *inst,
      const uint32_t *mask,
      const llvm::ArrayRef<CallLowering::RetLoc> returns
  );
  /// Follow move arguments to a non-move instruction.
  ConstRef<Value> GetMoveArg(ConstRef<MovInst> inst);
  /// Check if the value is exported from its defining block.
  bool IsExported(ConstRef<Inst> inst);

protected:
  /// Prepare LLVM globals.
  void PrepareGlobals();
  /// Handle PHI nodes in successor blocks.
  void HandleSuccessorPHI(const Block *block);
  /// Prepares the dag for instruction selection.
  void CodeGenAndEmitDAG();
  /// Creates a MachineBasicBlock with MachineInstrs.
  void DoInstructionSelection();

protected:
  /// Report an error at an instruction.
  [[noreturn]] void Error(const Inst *i, const std::string_view &message);
  /// Report an error in a function.
  [[noreturn]] void Error(const Func *f, const std::string_view &message);

protected:
  /// Lowers a call instructions.
  void LowerCall(const CallInst *inst);
  /// Lowers a tail call instruction.
  void LowerTailCall(const TailCallInst *inst);
  /// Lowers an invoke instruction.
  void LowerInvoke(const InvokeInst *inst);
  /// Lowers a binary instruction.
  void LowerBinary(const Inst *inst, unsigned op);
  /// Lowers a binary integer or float operation.
  void LowerBinary(const Inst *inst, unsigned sop, unsigned fop);
  /// Lowers a unary instruction.
  void LowerUnary(const UnaryInst *inst, unsigned opcode);
  /// Lowers a conditional jump true instruction.
  void LowerJCC(const JumpCondInst *inst);
  /// Lowers a jump instruction.
  void LowerJMP(const JumpInst *inst);
  /// Lowers a switch.
  void LowerSwitch(const SwitchInst *inst);
  /// Lowers a load.
  void LowerLD(const LoadInst *inst);
  /// Lowers a store.
  void LowerST(const StoreInst *inst);
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
  /// Lowers a any extend instruction.
  void LowerXExt(const XExtInst *inst);
  /// Lowers a truncate instruction.
  void LowerTrunc(const TruncInst *inst);
  /// Lowers an alloca instruction.
  void LowerAlloca(const AllocaInst *inst);
  /// Lowers a select instruction.
  void LowerSelect(const SelectInst *inst);
  /// Lowers an undefined instruction.
  void LowerUndef(const UndefInst *inst);
  /// Lowers an overflow check instruction.
  void LowerALUO(const OverflowInst *inst, unsigned op);

protected:
  /// Lowers a call instruction.
  virtual void LowerCallSite(llvm::SDValue chain, const CallSite *call) = 0;
  /// Find the calling convention of a call.
  std::pair<bool, llvm::CallingConv::ID> GetCallingConv(
      const Func *caller,
      const CallSite *call
  );

protected:
  /// Target library info.
  llvm::TargetLibraryInfo *libInfo_;

  /// Program to lower.
  const Prog *prog_;

  /// Dummy debug location.
  llvm::DebugLoc DL_;
  /// Dummy SelectionDAG debug location.
  llvm::SDLoc SDL_;

  /// Current module.
  llvm::Module *M_;
  /// Void type.
  llvm::Type *voidTy_;
  /// Void pointer type.
  llvm::Type *i8PtrTy_;
  /// Dummy function type.
  llvm::FunctionType *funcTy_;

  /// Current function.
  const Func *func_;
  /// Current LLVM function.
  llvm::Function *F_;
  /// Current basic block.
  llvm::MachineBasicBlock *MBB_;
  /// Current insertion point.
  llvm::MachineBasicBlock::iterator insert_;
  /// Per-function live variable info.
  std::unique_ptr<LiveVariables> lva_;
  /// Mapping from nodes to values.
  std::unordered_map<ConstRef<Inst>, llvm::SDValue> values_;
  /// Mapping from nodes to registers.
  std::unordered_map<ConstRef<Inst>, unsigned> regs_;
  /// Mapping from stack_object indices to llvm stack objects.
  llvm::DenseMap<unsigned, unsigned> stackIndices_;
  /// Frame start index, if necessary.
  int frameIndex_;
  /// Argument frame indices.
  llvm::DenseMap<unsigned, int> args_;

  /// Pending primitives to be exported.
  std::vector<std::pair<unsigned, llvm::SDValue>> pendingPrimValues_;
  /// Pending primitive instructions to be exported.
  std::unordered_map<ConstRef<Inst>, unsigned> pendingPrimInsts_;
  /// Pending value-producing instructions to be exported.
  std::unordered_map<ConstRef<Inst>, unsigned> pendingValueInsts_;
};
