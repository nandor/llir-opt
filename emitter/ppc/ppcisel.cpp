// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.


#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/IntrinsicsPowerPC.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/MachineJumpTableInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/PowerPC/PPCISelLowering.h>
#include <llvm/Target/PowerPC/PPCMachineFunctionInfo.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/data.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"
#include "emitter/ppc/ppccall.h"
#include "emitter/ppc/ppcisel.h"

namespace ISD = llvm::ISD;
namespace PPCISD = llvm::PPCISD;
namespace PPC = llvm::PPC;



// -----------------------------------------------------------------------------
char PPCISel::ID;

// -----------------------------------------------------------------------------
PPCISel::PPCISel(
    llvm::PPCTargetMachine *TM,
    llvm::PPCSubtarget *STI,
    const llvm::PPCInstrInfo *TII,
    const llvm::PPCRegisterInfo *TRI,
    const llvm::TargetLowering *TLI,
    llvm::TargetLibraryInfo *LibInfo,
    const Prog &prog,
    llvm::CodeGenOpt::Level OL,
    bool shared)
  : DAGMatcher(*TM, new llvm::SelectionDAG(*TM, OL), OL, TLI, TII)
  , PPCDAGMatcher(*TM, OL, STI)
  , ISel(ID, prog, LibInfo)
  , TM_(TM)
  , STI_(STI)
  , TRI_(TRI)
  , trampoline_(nullptr)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
llvm::SDValue PPCISel::LoadRegArch(ConstantReg::Kind reg)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCISel::LowerArch(const Inst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::SDValue PPCISel::LowerCallee(ConstRef<Inst> inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCISel::LowerCallSite(SDValue chain, const CallSite *call)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCISel::LowerSyscall(const SyscallInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCISel::LowerClone(const CloneInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCISel::LowerReturn(const ReturnInst *retInst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCISel::LowerArguments(bool hasVAStart)
{
  PPCCall lowering(func_);
  if (hasVAStart) {
    LowerVASetup(lowering);
  }
  LowerArgs(lowering);
}

// -----------------------------------------------------------------------------
void PPCISel::LowerRaise(const RaiseInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCISel::LowerSet(const SetInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCISel::LowerVASetup(const PPCCall &ci)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::ScheduleDAGSDNodes *PPCISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}
