// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.


#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/IntrinsicsRISCV.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/MachineJumpTableInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/RISCV/RISCVISelLowering.h>
#include <llvm/Target/RISCV/RISCVMachineFunctionInfo.h>

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
#include "emitter/riscv/riscvcall.h"
#include "emitter/riscv/riscvisel.h"

namespace ISD = llvm::ISD;
namespace RISCVISD = llvm::RISCVISD;
namespace RISCV = llvm::RISCV;



// -----------------------------------------------------------------------------
char RISCVISel::ID;

// -----------------------------------------------------------------------------
RISCVISel::RISCVISel(
    llvm::RISCVTargetMachine *TM,
    llvm::RISCVSubtarget *STI,
    const llvm::RISCVInstrInfo *TII,
    const llvm::RISCVRegisterInfo *TRI,
    const llvm::TargetLowering *TLI,
    llvm::TargetLibraryInfo *LibInfo,
    const Prog &prog,
    llvm::CodeGenOpt::Level OL,
    bool shared)
  : DAGMatcher(*TM, new llvm::SelectionDAG(*TM, OL), OL, TLI, TII)
  , RISCVDAGMatcher(*TM, OL, STI)
  , ISel(ID, prog, LibInfo)
  , TM_(TM)
  , STI_(STI)
  , TRI_(TRI)
  , trampoline_(nullptr)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
llvm::SDValue RISCVISel::LoadRegArch(ConstantReg::Kind reg)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerArch(const Inst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::SDValue RISCVISel::LowerCallee(ConstRef<Inst> inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerCallSite(SDValue chain, const CallSite *call)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerSyscall(const SyscallInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerClone(const CloneInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerReturn(const ReturnInst *retInst)
{
  llvm::SmallVector<SDValue, 6> ops;
  ops.push_back(SDValue());

  SDValue flag;
  SDValue chain = GetExportRoot();

  RISCVCall ci(retInst);
  for (unsigned i = 0, n = retInst->arg_size(); i < n; ++i) {
    ConstRef<Inst> arg = retInst->arg(i);
    SDValue fullValue = GetValue(arg);
    const MVT argVT = GetVT(arg.GetType());
    const CallLowering::RetLoc &ret = ci.Return(i);
    for (unsigned j = 0, m = ret.Parts.size(); j < m; ++j) {
      auto &part = ret.Parts[j];

      SDValue argValue;
      if (m == 1) {
        if (argVT != part.VT) {
          argValue = CurDAG->getAnyExtOrTrunc(fullValue, SDL_, part.VT);
        } else {
          argValue = fullValue;
        }
      } else {
        argValue = CurDAG->getNode(
            ISD::EXTRACT_ELEMENT,
            SDL_,
            part.VT,
            fullValue,
            CurDAG->getConstant(j, SDL_, part.VT)
        );
      }

      chain = CurDAG->getCopyToReg(chain, SDL_, part.Reg, argValue, flag);
      ops.push_back(CurDAG->getRegister(part.Reg, part.VT));
      flag = chain.getValue(1);
    }
  }

  ops[0] = chain;
  if (flag.getNode()) {
    ops.push_back(flag);
  }

  CurDAG->setRoot(CurDAG->getNode(
      RISCVISD::RET_FLAG,
      SDL_,
      MVT::Other,
      ops
  ));
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerArguments(bool hasVAStart)
{
  RISCVCall lowering(func_);
  if (hasVAStart) {
    LowerVASetup(lowering);
  }
  LowerArgs(lowering);
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerRaise(const RaiseInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerSetSP(SDValue value)
{
  CurDAG->setRoot(CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      RISCV::SP,
      value
  ));
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerSet(const SetInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerVASetup(const RISCVCall &ci)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::ScheduleDAGSDNodes *RISCVISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}
