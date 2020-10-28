// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.


#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/MachineJumpTableInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/AArch64/AArch64ISelLowering.h>

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
#include "emitter/aarch64/aarch64call.h"
#include "emitter/aarch64/aarch64isel.h"

namespace AArch64ISD = llvm::AArch64ISD;
namespace AArch64 = llvm::AArch64;



// -----------------------------------------------------------------------------
char AArch64ISel::ID;

// -----------------------------------------------------------------------------
AArch64ISel::AArch64ISel(
    llvm::AArch64TargetMachine *TM,
    llvm::AArch64Subtarget *STI,
    const llvm::AArch64InstrInfo *TII,
    const llvm::AArch64RegisterInfo *TRI,
    const llvm::TargetLowering *TLI,
    llvm::TargetLibraryInfo *LibInfo,
    const Prog *prog,
    llvm::CodeGenOpt::Level OL,
    bool shared)
  : DAGMatcher(*TM, new llvm::SelectionDAG(*TM, OL), OL, TLI, TII)
  , AArch64DAGMatcher(*TM, OL, STI)
  , ISel(ID, prog, LibInfo)
  , TM_(TM)
  , TRI_(TRI)
  , trampoline_(nullptr)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
llvm::SDValue AArch64ISel::LoadReg(ConstantReg::Kind reg)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerArch(const Inst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerCallSite(SDValue chain, const CallSite *call)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerSyscall(const SyscallInst *inst)
{
  static unsigned kRegs[] = {
      AArch64::X0, AArch64::X1, AArch64::X2,
      AArch64::X3, AArch64::X4, AArch64::X5
  };

  llvm::SmallVector<SDValue, 7> ops;
  SDValue chain = CurDAG->getRoot();

  // Lower the syscall number.
  ops.push_back(CurDAG->getTargetConstant(0, SDL_, MVT::i32));

  // Lower arguments.
  unsigned args = 0;
  {
    unsigned n = sizeof(kRegs) / sizeof(kRegs[0]);
    for (ConstRef<Inst> arg : inst->args()) {
      if (args >= n) {
        Error(inst, "too many arguments to syscall");
      }

      SDValue value = GetValue(arg);
      if (arg.GetType() != Type::I64) {
        Error(inst, "invalid syscall argument");
      }
      ops.push_back(CurDAG->getRegister(kRegs[args], MVT::i64));
      chain = CurDAG->getCopyToReg(chain, SDL_, kRegs[args++], value);
    }
  }

  /// Lower to the syscall.
  {
    ops.push_back(CurDAG->getRegister(AArch64::X8, MVT::i64));

    chain = CurDAG->getCopyToReg(
        chain,
        SDL_,
        AArch64::X8,
        GetValue(inst->GetSyscall())
    );

    ops.push_back(chain);

    chain = SDValue(CurDAG->getMachineNode(
        AArch64::SVC,
        SDL_,
        CurDAG->getVTList(MVT::Other, MVT::Glue),
        ops
    ), 0);
  }

  /// Copy the return value into a vreg and export it.
  {
    if (auto type = inst->GetType()) {
      if (*type != Type::I64) {
        Error(inst, "invalid syscall type");
      }

      chain = CurDAG->getCopyFromReg(
          chain,
          SDL_,
          AArch64::X0,
          MVT::i64,
          chain.getValue(1)
      ).getValue(1);

      Export(inst, chain.getValue(0));
    }
  }

  CurDAG->setRoot(chain);
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerClone(const CloneInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerReturn(const ReturnInst *retInst)
{
  llvm::SmallVector<SDValue, 6> ops;
  ops.push_back(SDValue());

  SDValue flag;
  SDValue chain = GetExportRoot();

  AArch64Call ci(retInst);
  for (unsigned i = 0, n = retInst->arg_size(); i < n; ++i) {
    ConstRef<Inst> arg = retInst->arg(i);
    const CallLowering::RetLoc &ret = ci.Return(i);
    chain = CurDAG->getCopyToReg(chain, SDL_, ret.Reg, GetValue(arg), flag);
    ops.push_back(CurDAG->getRegister(ret.Reg, ret.VT));
    flag = chain.getValue(1);
  }

  ops[0] = chain;
  if (flag.getNode()) {
    ops.push_back(flag);
  }

  CurDAG->setRoot(CurDAG->getNode(
      AArch64ISD::RET_FLAG,
      SDL_,
      MVT::Other,
      ops
  ));
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerArguments(bool hasVAStart)
{
  AArch64Call lowering(func_);
  if (hasVAStart) {
    LowerVASetup(lowering);
  }
  LowerArgs(lowering);
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerVAStart(const VAStartInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerRaise(const RaiseInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerSet(const SetInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerVASetup(const AArch64Call &ci)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::ScheduleDAGSDNodes *AArch64ISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}
