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

class Target;



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
  ISel(
      char &ID,
      const Target &target,
      const Prog &prog,
      llvm::TargetLibraryInfo &libInfo,
      llvm::CodeGenOpt::Level ol
  );

private:
  /// Return the name of the pass.
  llvm::StringRef getPassName() const override;
  /// Requires MachineModuleInfo.
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  /// Set up the instruction selector to run on a module.
  bool doInitialization(llvm::Module &M) override;
  /// Creates MachineFunctions from LLIR.
  bool runOnModule(llvm::Module &M) override;

protected:
  /// Lowers an instruction.
  void Lower(const Inst *inst);

protected:
  /// Start lowering a function.
  virtual void Lower(llvm::MachineFunction &mf) = 0;

  /// Reads the value from an architecture-specific register.
  virtual SDValue GetRegArch(Register reg) = 0;

  /// Lowers a system call instruction.
  virtual void LowerSyscall(const SyscallInst *inst) = 0;
  /// Lowers a process clone instruction.
  virtual void LowerClone(const CloneInst *inst) = 0;
  /// Lowers a return.
  virtual void LowerReturn(const ReturnInst *inst) = 0;
  /// Lowers an indirect jump.
  virtual void LowerRaise(const RaiseInst *inst) = 0;
  /// Lowers a spawn instruction.
  virtual void LowerSpawn(const SpawnInst *inst) = 0;
  /// Lowers a landing pad.
  virtual void LowerLandingPad(const LandingPadInst *inst) = 0;
  /// Lowers a fixed register set instruction.
  virtual void LowerSet(const SetInst *inst) = 0;
  /// Lowers a target-specific instruction.
  virtual void LowerArch(const Inst *inst) = 0;

  /// Lowers variable argument list frame setup.
  virtual void LowerArguments(bool hasVAStart) = 0;

  /// Returns a reference to the current DAG.
  virtual llvm::SelectionDAG &GetDAG() const = 0;

  /// Returns the stack pointer.
  virtual llvm::Register GetStackRegister() const = 0;

  /// Target-specific preprocessing step.
  virtual void PreprocessISelDAG() = 0;
  /// Target-specific post-processing step.
  virtual void PostprocessISelDAG() = 0;
  /// Target-specific instruction selection.
  virtual void Select(SDNode *node) = 0;

  /// Finalize the lowering.
  virtual bool Finalize(llvm::MachineFunction &MF) { return true; }

protected:
  /// Lovers a register value.
  SDValue LoadReg(Register reg);
  /// Lowers an offset reference to a global.
  llvm::SDValue LowerGlobal(const Global &val, Type type);
  /// Lowers a global value.
  llvm::SDValue LowerGlobal(const Global &val, int64_t offset, Type type);
  /// Lowers all arguments.
  void LowerArgs(const CallLowering &lowering);
  /// Lower all return values.
  std::pair<llvm::SDValue, llvm::SDValue>
  LowerRets(
      llvm::SDValue chain,
      const CallLowering &lowering,
      const ReturnInst *ret,
      llvm::SmallVectorImpl<SDValue> &ops
  );
  /// Lower all return values.
  std::pair<llvm::SDValue, llvm::SDValue>
  LowerRaises(
      llvm::SDValue chain,
      const CallLowering &lowering,
      const RaiseInst *ret,
      llvm::SmallVectorImpl<llvm::Register> &regs,
      llvm::SDValue glue
  );
  /// Lowers a landing pad.
  void LowerPad(
      const CallLowering &lowering,
      const LandingPadInst *inst
  );

  using RegParts = llvm::SmallVector<std::pair<llvm::Register, llvm::MVT>, 2>;
  using ExportList = std::vector<std::pair<RegParts, SDValue>>;

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
  RegParts ExportValue(SDValue value);

  /// Creates a register for an instruction's result.
  RegParts AssignVReg(ConstRef<Inst> inst);

  /// Lower an inline asm sequence.
  SDValue LowerInlineAsm(
      unsigned opcode,
      SDValue chain,
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
  SDValue LowerExpr(const Expr &expr, Type type);

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
      const CallSite *inst
  );
  /// Follow move arguments to a non-move instruction.
  ConstRef<Value> GetMoveArg(ConstRef<MovInst> inst);
  /// Check if the value is exported from its defining block.
  bool IsExported(ConstRef<Inst> inst);
  /// Return the pointer type.
  MVT GetPointerType() const;

protected:
  /// Prepare a function.
  virtual void PrepareFunction(const Func &func, llvm::MachineFunction &MF) {}
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
  /// Lowers a vararg frame setup instruction.
  void LowerVAStart(const VaStartInst *inst);
  /// Lowers a call instructions.
  void LowerCall(const CallInst *inst);
  /// Lowers a tail call instruction.
  void LowerTailCall(const TailCallInst *inst);
  /// Lowers an invoke instruction.
  void LowerInvoke(const InvokeInst *inst);
  /// Lowers a frame call instruction.
  void LowerFrameCall(const FrameCallInst *inst);
  /// Lowers a binary instruction.
  void LowerBinary(const Inst *inst, unsigned op);
  /// Lowers a binary integer or float operation.
  void LowerBinary(const Inst *inst, unsigned sop, unsigned fop);
  /// Lowers a shift instruction.
  void LowerShift(const Inst *inst, unsigned op);
  /// Lowers a unary instruction.
  void LowerUnary(const UnaryInst *inst, unsigned opcode);
  /// Lowers a conditional jump true instruction.
  void LowerJUMP_COND(const JumpCondInst *inst);
  /// Lowers a jump instruction.
  void LowerJUMP(const JumpInst *inst);
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
  /// Lowers a debug trap instruction.
  void LowerDebugTrap(const DebugTrapInst *inst);
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
  void LowerALUO(const BinaryInst *inst, unsigned op);
  /// Lowers a fixed register get instruction.
  void LowerGet(const GetInst *inst);
  /// Lowers a bit cast instruction.
  void LowerBitCast(const BitCastInst *inst);

protected:
  /// Lowers a call instruction.
  virtual void LowerCallSite(llvm::SDValue chain, const CallSite *call) = 0;
  /// Find the calling convention of a call.
  std::pair<bool, llvm::CallingConv::ID> GetCallingConv(
      const Func *caller,
      const CallSite *call
  );
  /// Lower the arguments to a call.
  SDValue LowerCallArguments(
      SDValue chain,
      const CallSite *call,
      CallLowering &ci,
      llvm::SmallVectorImpl<std::pair<unsigned, SDValue>> &regs
  );

  /// Lower values returned from a call.
  std::pair<SDValue, SDValue> LowerReturns(
      SDValue chain,
      SDValue inFlag,
      const CallSite *call,
      llvm::SmallVectorImpl<CallLowering::RetLoc> &returns,
      llvm::SmallVectorImpl<SDValue> &regs,
      llvm::SmallVectorImpl<std::pair<ConstRef<Inst>, SDValue>> &values
  );

protected:
  /// Reference to the target.
  const Target &target_;
  /// Program to lower.
  const Prog &prog_;
  /// Target library info.
  llvm::TargetLibraryInfo &libInfo_;

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
  /// Chosen optimisation level.
  llvm::CodeGenOpt::Level ol_;

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
  std::unordered_map<ConstRef<Inst>, RegParts> regs_;
  /// Mapping from stack_object indices to llvm stack objects.
  llvm::DenseMap<unsigned, unsigned> stackIndices_;
  /// Frame start index, if necessary.
  int frameIndex_;

  /// Pending primitives to be exported.
  std::vector<std::pair<RegParts, llvm::SDValue>> pendingPrimValues_;
  /// Pending primitive instructions to be exported.
  std::unordered_map<ConstRef<Inst>, RegParts> pendingPrimInsts_;
  /// Pending value-producing instructions to be exported.
  std::unordered_map<ConstRef<Inst>, RegParts> pendingValueInsts_;
};
