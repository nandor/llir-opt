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
#include <llvm/Target/AArch64/AArch64MachineFunctionInfo.h>

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

namespace ISD = llvm::ISD;
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
  , STI_(STI)
  , TRI_(TRI)
  , trampoline_(nullptr)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
llvm::SDValue AArch64ISel::LowerGetFS()
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
  auto &RegInfo = MF->getRegInfo();
  auto &TLI = GetTargetLowering();

  // Copy in the new stack pointer and code pointer.
  SDValue chain;
  unsigned callee = RegInfo.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  chain = CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      callee,
      GetValue(inst->GetCallee()),
      chain
  );
  unsigned arg = RegInfo.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  chain = CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      arg,
      GetValue(inst->GetArg()),
      chain
  );

  // Copy in other registers.
  auto CopyReg = [&](ConstRef<Inst> arg, unsigned reg) {
    chain = CurDAG->getCopyToReg(
        CurDAG->getRoot(),
        SDL_,
        reg,
        GetValue(arg),
        chain
    );
  };

  CopyReg(inst->GetFlags(), AArch64::X0);
  CopyReg(inst->GetStack(), AArch64::X1);
  CopyReg(inst->GetPTID(), AArch64::X2);
  CopyReg(inst->GetTLS(), AArch64::X3);
  CopyReg(inst->GetCTID(), AArch64::X4);

  chain = LowerInlineAsm(
      "and x1, x1, #-16\n"
      "stp $1, $2, [x1,#-16]!\n"
      "mov x8, #220\n"
      "svc #0\n"
      "cbnz x0, 1f\n"
      "ldp x1, x0, [sp], #16\n"
      "blr x1\n"
      "mov x8, #93\n"
      "svc #0\n"
      "1:\n",
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      {
          callee, arg,
          AArch64::X0, AArch64::X1, AArch64::X2, AArch64::X3, AArch64::X4
      },
      { AArch64::NZCV },
      { AArch64::X0 },
      chain.getValue(1)
  );

  /// Copy the return value into a vreg and export it.
  {
    if (inst->GetType() != Type::I64) {
      Error(inst, "invalid clone type");
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

  // Update the root.
  CurDAG->setRoot(chain);
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
void AArch64ISel::LowerRaise(const RaiseInst *inst)
{
  auto &RegInfo = MF->getRegInfo();
  auto &TLI = GetTargetLowering();

  // Copy in the new stack pointer and code pointer.
  auto stk = RegInfo.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  SDValue stkNode = CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      stk,
      GetValue(inst->GetStack()),
      SDValue()
  );
  auto pc = RegInfo.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  SDValue pcNode = CurDAG->getCopyToReg(
      stkNode,
      SDL_,
      pc,
      GetValue(inst->GetTarget()),
      stkNode.getValue(1)
  );

  // Lower the values to return.
  SDValue glue = pcNode.getValue(1);
  SDValue chain = CurDAG->getRoot();
  llvm::SmallVector<llvm::Register, 4> regs{ stk, pc };
  if (auto cc = inst->GetCallingConv()) {
    AArch64Call ci(inst);
    for (unsigned i = 0, n = inst->arg_size(); i < n; ++i) {
      llvm::Register reg = ci.Return(i).Reg;
      regs.push_back(reg);
      chain = CurDAG->getCopyToReg(
          CurDAG->getRoot(),
          SDL_,
          reg,
          GetValue(inst->arg(i)),
          glue
      );
      glue = chain.getValue(1);
    }
  } else {
    if (!inst->arg_empty()) {
      Error(inst, "missing calling convention");
    }
  }

  CurDAG->setRoot(LowerInlineAsm(
      "mov sp, $0\n"
      "br  $1",
      0,
      regs,
      { },
      { },
      glue
  ));
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerSet(const SetInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerVASetup(const AArch64Call &ci)
{
  auto &MFI = MF->getFrameInfo();
  bool isWin64 = STI_->isCallingConvWin64(MF->getFunction().getCallingConv());

  if (!STI_->isTargetDarwin() || isWin64) {
    SaveVarArgRegisters(ci, isWin64);
  }

  // Set the index to the vararg object.
  unsigned offset = ci.GetFrameSize();
  offset = llvm::alignTo(offset, STI_->isTargetILP32() ? 4 : 8);
  FuncInfo_->setVarArgsStackIndex(MFI.CreateFixedObject(4, offset, true));

  if (MFI.hasMustTailInVarArgFunc()) {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
void AArch64ISel::SaveVarArgRegisters(const AArch64Call &ci, bool isWin64)
{
  auto &MFI = MF->getFrameInfo();
  auto ptrTy = GetPtrTy();

  llvm::SmallVector<SDValue, 8> memOps;

  auto unusedGPRs = ci.GetUnusedGPRs();
  unsigned gprSize = 8 * unusedGPRs.size();
  int gprIdx = 0;
  if (gprSize != 0) {
    if (isWin64) {
      gprIdx = MFI.CreateFixedObject(gprSize, -(int)gprSize, false);
      if (gprSize & 15) {
        MFI.CreateFixedObject(
            16 - (gprSize & 15),
            -(int)llvm::alignTo(gprSize, 16),
            false
        );
      }
    } else {
      gprIdx = MFI.CreateStackObject(gprSize, llvm::Align(8), false);
    }

    SDValue fidx = CurDAG->getFrameIndex(gprIdx, ptrTy);
    unsigned usedGPRs = ci.GetUsedGPRs().size();
    for (unsigned i = 0; i < unusedGPRs.size(); ++i) {
      unsigned vreg = MF->addLiveIn(unusedGPRs[i], &AArch64::GPR64RegClass);
      SDValue val = CurDAG->getCopyFromReg(
          CurDAG->getRoot(),
          SDL_,
          vreg,
          MVT::i64
      );

      llvm::MachinePointerInfo MPI;
      if (isWin64) {
        MPI = llvm::MachinePointerInfo::getFixedStack(*MF, gprIdx, i * 8);
      } else {
        MPI = llvm::MachinePointerInfo::getStack(*MF, (usedGPRs + i) * 8);
      }

      memOps.push_back(CurDAG->getStore(val.getValue(1), SDL_, val, fidx, MPI));
      fidx = CurDAG->getNode(
          ISD::ADD,
          SDL_,
          ptrTy,
          fidx,
          CurDAG->getConstant(8, SDL_, ptrTy)
      );
    }
  }
  FuncInfo_->setVarArgsGPRIndex(gprIdx);
  FuncInfo_->setVarArgsGPRSize(gprSize);

  if (Subtarget->hasFPARMv8() && !isWin64) {
    auto unusedFPRs = ci.GetUnusedFPRs();
    unsigned fprSize = 16 * unusedFPRs.size();
    int fprIdx = 0;
    if (fprSize != 0) {
      fprIdx = MFI.CreateStackObject(fprSize, llvm::Align(16), false);

      SDValue fidx = CurDAG->getFrameIndex(fprIdx, ptrTy);
      unsigned usedFPRs = ci.GetUsedFPRs().size();
      for (unsigned i = 0; i < unusedFPRs.size(); ++i) {
        unsigned vreg = MF->addLiveIn(unusedFPRs[i], &AArch64::FPR128RegClass);
        SDValue val = CurDAG->getCopyFromReg(
            CurDAG->getRoot(),
            SDL_,
            vreg,
            MVT::f128
        );
        memOps.push_back(CurDAG->getStore(
            val.getValue(1),
            SDL_,
            val,
            fidx,
            llvm::MachinePointerInfo::getStack(
                CurDAG->getMachineFunction(),
                (usedFPRs + i) * 16
            )
        ));

        fidx = CurDAG->getNode(
            ISD::ADD,
            SDL_,
            ptrTy,
            fidx,
            CurDAG->getConstant(16, SDL_, ptrTy)
        );
      }
    }
    FuncInfo_->setVarArgsFPRIndex(fprIdx);
    FuncInfo_->setVarArgsFPRSize(fprSize);
  }

  if (!memOps.empty()) {
    CurDAG->setRoot(CurDAG->getNode(
        ISD::TokenFactor,
        SDL_,
        MVT::Other,
        memOps
    ));
  }
}

// -----------------------------------------------------------------------------
llvm::ScheduleDAGSDNodes *AArch64ISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}
