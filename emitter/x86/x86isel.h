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
#include "core/analysis/live_variables.h"
#include "emitter/isel.h"
#include "emitter/x86/x86call.h"

class Data;
class Func;
class Inst;
class Prog;



/**
 * Custom pass to generate MIR from LLIR instead of LLVM IR.
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
      llvm::CodeGenOpt::Level OL,
      bool shared
  );

private:
  /// Creates MachineFunctions from LLIR.
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

  /// Lowers a refrence to a global.
  llvm::SDValue LowerGlobal(const Global *val, int64_t offset) override;
  /// Lovers a register value.
  llvm::SDValue LoadReg(ConstantReg::Kind reg) override;

  /// Lowers a call instructions.
  void LowerCall(const CallInst *inst) override;
  /// Lowers a tail call instruction.
  void LowerTailCall(const TailCallInst *inst) override;
  /// Lowers an invoke instruction.
  void LowerInvoke(const InvokeInst *inst) override;
  /// Lowers a tail invoke instruction.
  void LowerTailInvoke(const TailInvokeInst *inst) override;
  /// Lowers a system call instruction.
  void LowerSyscall(const SyscallInst *inst) override;
  /// Lowers a return.
  void LowerReturn(const ReturnInst *inst) override;
  /// Lowers a compare and exchange instructions.
  void LowerCmpXchg(const CmpXchgInst *inst) override;
  /// Lowers a RDTSC instruction.
  void LowerRDTSC(const RdtscInst *inst) override;
  /// Lowers a FNStCw instruction.
  void LowerFNStCw(const FNStCwInst *inst) override;
  /// Lowers a FLdCw instruction.
  void LowerFLdCw(const FLdCwInst *inst) override;
  /// Lowers a vararg frame setup instruction.
  void LowerVAStart(const VAStartInst *inst) override;
  /// Lowers a switch.
  void LowerSwitch(const SwitchInst *inst) override;
  /// Lowers a fixed register set instruction.
  void LowerSet(const SetInst *inst) override;

  /// Lowers an argument.
  void LowerArg(const Func &func, X86Call::Loc &argLoc);
  /// Lowers variable argument list frame setup.
  void LowerVASetup(const Func &func, X86Call &ci);
  /// Creates a register for an instruction's result.
  unsigned AssignVReg(const Inst *inst);

private:
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

  llvm::MVT GetPtrTy() const override { return llvm::MVT::i64; }

private:
  const llvm::X86TargetMachine &getTargetMachine() const override {
    return *TM_;
  }

  llvm::SelectionDAG &GetDAG() override { return *CurDAG; }
  llvm::CodeGenOpt::Level GetOptLevel() override { return OptLevel; }
  void PreprocessISelDAG() override { return X86DAGMatcher::PreprocessISelDAG(); }
  void Select(SDNode *node) override { return X86DAGMatcher::Select(node); }
  llvm::MachineRegisterInfo &GetRegisterInfo() override { return MF->getRegInfo(); }

  const llvm::TargetLowering &GetTargetLowering() override { return *TLI; }

  llvm::ScheduleDAGSDNodes *CreateScheduler() override;

private:
  /// Target machine.
  const llvm::X86TargetMachine *TM_;
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
  /// Dummy debug location.
  llvm::DebugLoc DL_;
  /// Current module.
  llvm::Module *M;
  /// Current LLVM function.
  llvm::Function *F;
  /// Variables live on exit - used to implement sets of regs.
  std::set<unsigned> liveOnExit_;
  /// Per-function live variable info.
  std::unique_ptr<LiveVariables> lva_;
  /// Frame start index, if necessary.
  int frameIndex_;
  /// Generate OCaml trampoline, if necessary.
  llvm::Function *trampoline_;
  /// Flag to indicate whether the target is a shared object.
  bool shared_;
};
