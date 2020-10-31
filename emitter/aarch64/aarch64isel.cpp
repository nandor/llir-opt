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
  auto &RegInfo = MF->getRegInfo();
  auto reg = RegInfo.createVirtualRegister(TLI->getRegClassFor(MVT::i64));
  auto node = LowerInlineAsm(
      "mrs $0, tpidr_el0",
      0,
      { },
      { },
      { reg }
  );

  auto copy = CurDAG->getCopyFromReg(
      node.getValue(0),
      SDL_,
      reg,
      MVT::i64,
      node.getValue(1)
  );

  CurDAG->setRoot(copy.getValue(1));
  return copy.getValue(0);
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerArch(const Inst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::SDValue AArch64ISel::LowerCallee(ConstRef<Inst> inst)
{
  if (ConstRef<MovInst> movInst = ::cast_or_null<MovInst>(inst)) {
    ConstRef<Value> movArg = GetMoveArg(movInst.Get());
    switch (movArg->GetKind()) {
      case Value::Kind::INST: {
        return GetValue(::cast<Inst>(movArg));
        break;
      }
      case Value::Kind::GLOBAL: {
        const Global &movGlobal = *::cast<Global>(movArg);
        switch (movGlobal.GetKind()) {
          case Global::Kind::BLOCK: {
            llvm_unreachable("invalid call argument");
          }
          case Global::Kind::FUNC:
          case Global::Kind::ATOM:
          case Global::Kind::EXTERN: {
            auto name = movGlobal.getName();
            if (auto *GV = M_->getNamedValue(name)) {
              return CurDAG->getTargetGlobalAddress(
                  GV,
                  SDL_,
                  MVT::i64,
                  0,
                  llvm::AArch64II::MO_NO_FLAG
              );
            } else {
              Error(inst.Get(), "Unknown symbol '" + std::string(name) + "'");
            }
            break;
          }
        }
        llvm_unreachable("invalid global kind");
      }
      case Value::Kind::EXPR:
      case Value::Kind::CONST: {
        llvm_unreachable("invalid call argument");
      }
    }
    llvm_unreachable("invalid value kind");
  } else {
    return GetValue(inst);
  }
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerCallSite(SDValue chain, const CallSite *call)
{
  const Block *block = call->getParent();
  const Func *func = block->getParent();
  auto ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();

  // Analyse the arguments, finding registers for them.
  bool isVarArg = call->IsVarArg();
  bool isTailCall = call->Is(Inst::Kind::TCALL);
  bool isInvoke = call->Is(Inst::Kind::INVOKE);
  bool wasTailCall = isTailCall;
  AArch64Call locs(call);

  // Find the number of bytes allocated to hold arguments.
  unsigned stackSize = locs.GetFrameSize();

  // Compute the stack difference for tail calls.
  int fpDiff = 0;
  if (isTailCall) {
    AArch64Call callee(func);
    int bytesToPop;
    switch (func->GetCallingConv()) {
      default: {
        llvm_unreachable("invalid C calling convention");
      }
      case CallingConv::C: {
        if (func->IsVarArg()) {
          bytesToPop = callee.GetFrameSize();
        } else {
          bytesToPop = 0;
        }
        break;
      }
      case CallingConv::SETJMP:
      case CallingConv::CAML:
      case CallingConv::CAML_ALLOC:
      case CallingConv::CAML_GC: {
        bytesToPop = 0;
        break;
      }
    }
    fpDiff = bytesToPop - static_cast<int>(stackSize);
  }

  if (isTailCall && fpDiff) {
    // TODO: some tail calls can still be lowered.
    wasTailCall = true;
    isTailCall = false;
  }

  // Flag to indicate whether the call needs CALLSEQ_START/CALLSEQ_END.
  const bool needsAdjust = !isTailCall;

  // Calls from OCaml to C need to go through a trampoline.
  auto [needsTrampoline, cc] = GetCallingConv(func, call);
  const uint32_t *mask = TRI_->getCallPreservedMask(*MF, cc);

  // Instruction bundle starting the call.
  if (needsAdjust) {
    chain = CurDAG->getCALLSEQ_START(chain, stackSize, 0, SDL_);
  }

  // Identify registers and stack locations holding the arguments.
  llvm::SmallVector<std::pair<unsigned, SDValue>, 8> regArgs;
  llvm::SmallVector<SDValue, 8> memOps;
  SDValue stackPtr;
  for (auto it = locs.arg_begin(); it != locs.arg_end(); ++it) {
    SDValue argument = GetValue(it->Value);
    switch (it->Kind) {
      case CallLowering::ArgLoc::Kind::REG: {
        regArgs.emplace_back(it->Reg, argument);
        break;
      }
      case CallLowering::ArgLoc::Kind::STK: {
        if (!stackPtr.getNode()) {
          stackPtr = CurDAG->getCopyFromReg(
              chain,
              SDL_,
              AArch64::SP,
              ptrTy
          );
        }

        SDValue memOff = CurDAG->getNode(
            ISD::ADD,
            SDL_,
            ptrTy,
            stackPtr,
            CurDAG->getIntPtrConstant(it->Idx, SDL_)
        );

        memOps.push_back(CurDAG->getStore(
            chain,
            SDL_,
            argument,
            memOff,
            llvm::MachinePointerInfo::getStack(*MF, it->Idx)
        ));

        break;
      }
    }
  }

  if (!memOps.empty()) {
    chain = CurDAG->getNode(ISD::TokenFactor, SDL_, MVT::Other, memOps);
  }

  if (isTailCall) {
    // Shuffle arguments on the stack.
    for (auto it = locs.arg_begin(); it != locs.arg_end(); ++it) {
      switch (it->Kind) {
        case CallLowering::ArgLoc::Kind::REG: {
          continue;
        }
        case CallLowering::ArgLoc::Kind::STK: {
          llvm_unreachable("not implemented");
          break;
        }
      }
    }

    // Store the return address.
    if (fpDiff) {
      llvm_unreachable("not implemented");
    }
  }

  // Find the callee.
  SDValue callee;
  if (needsTrampoline) {
    // If call goes through a trampoline, replace the callee
    // and add the original one as the argument passed through $rax.
    if (!trampoline_) {
      trampoline_ = llvm::Function::Create(
          funcTy_,
          GlobalValue::ExternalLinkage,
          0,
          "caml_c_call",
          M_
      );
    }
    regArgs.emplace_back(AArch64::X15, GetValue(call->GetCallee()));
    callee = CurDAG->getTargetGlobalAddress(
        trampoline_,
        SDL_,
        MVT::i64,
        0,
        llvm::AArch64II::MO_NO_FLAG
    );
  } else {
    callee = LowerCallee(call->GetCallee());
  }

  // Prepare arguments in registers.
  SDValue inFlag;
  for (const auto &reg : regArgs) {
    chain = CurDAG->getCopyToReg(
        chain,
        SDL_,
        reg.first,
        reg.second,
        inFlag
    );
    inFlag = chain.getValue(1);
  }

  // Finish the call here for tail calls.
  if (needsAdjust && isTailCall) {
    chain = CurDAG->getCALLSEQ_END(
        chain,
        CurDAG->getIntPtrConstant(stackSize, SDL_, true),
        CurDAG->getIntPtrConstant(0, SDL_, true),
        inFlag,
        SDL_
    );
    inFlag = chain.getValue(1);
  }

  // Create the DAG node for the Call.
  llvm::SmallVector<SDValue, 8> ops;
  ops.push_back(chain);
  ops.push_back(callee);
  if (isTailCall) {
    ops.push_back(CurDAG->getTargetConstant(fpDiff, SDL_, MVT::i32));
  }
  for (const auto &reg : regArgs) {
    ops.push_back(CurDAG->getRegister(
        reg.first,
        reg.second.getValueType()
    ));
  }
  ops.push_back(CurDAG->getRegisterMask(mask));

  // Finalize the call node.
  if (inFlag.getNode()) {
    ops.push_back(inFlag);
  }

  // Generate a call or a tail call.
  SDVTList nodeTypes = CurDAG->getVTList(MVT::Other, MVT::Glue);
  if (isTailCall) {
    MF->getFrameInfo().setHasTailCall();
    CurDAG->setRoot(CurDAG->getNode(
        AArch64ISD::TC_RETURN,
        SDL_,
        nodeTypes,
        ops
    ));
  } else {
    chain = CurDAG->getNode(AArch64ISD::CALL, SDL_, nodeTypes, ops);
    inFlag = chain.getValue(1);

    // Find the register to store the return value in.
    std::vector<CallLowering::RetLoc> returns;
    std::vector<bool> used(call->type_size(), wasTailCall);
    if (wasTailCall || !call->use_empty()) {
      for (const Use &use : call->uses()) {
        used[(*use).Index()] = true;
      }
      for (unsigned i = 0, n = call->type_size(); i < n; ++i) {
        if (used[i]) {
          returns.push_back(locs.Return(i));
        }
      }
    }

    // Generate a GC_FRAME before the call, if needed.
    if (call->HasAnnot<CamlFrame>() && !isTailCall) {
      chain = LowerGCFrame(chain, inFlag, call, mask, returns);
      inFlag = chain.getValue(1);
    }

    if (needsAdjust) {
      chain = CurDAG->getCALLSEQ_END(
          chain,
          CurDAG->getIntPtrConstant(stackSize, SDL_, true),
          CurDAG->getIntPtrConstant(0, SDL_, true),
          inFlag,
          SDL_
      );
      inFlag = chain.getValue(1);
    }

    // Lower the return value.
    std::vector<SDValue> tailReturns;
    for (unsigned i = 0, n = call->type_size(); i < n; ++i) {
      // Export used return values.
      const auto &retLoc = locs.Return(i);
      if (!used[i]) {
        continue;
      }

      // Find the physical reg where the return value is stored.
      if (wasTailCall) {
        /// Copy the return value into a vreg.
        SDValue copy = CurDAG->getCopyFromReg(
            chain,
            SDL_,
            retLoc.Reg,
            retLoc.VT,
            inFlag
        );
        chain = copy.getValue(1);
        inFlag = copy.getValue(2);

        /// If this was a tailcall, forward to return.
        tailReturns.push_back(copy.getValue(0));
      } else {
        // Regular call with a return which is used - expose it.
        SDValue copy = CurDAG->getCopyFromReg(
            chain,
            SDL_,
            retLoc.Reg,
            retLoc.VT,
            inFlag
        );
        chain = copy.getValue(1);
        inFlag = copy.getValue(2);

        // Otherwise, expose the value.
        Export(call->GetSubValue(i), copy.getValue(0));
      }
    }

    if (wasTailCall) {
      llvm::SmallVector<SDValue, 6> returns;
      returns.push_back(chain);
      returns.push_back(CurDAG->getTargetConstant(0, SDL_, MVT::i32));
      for (auto &ret : tailReturns) {
        returns.push_back(ret);
      }

      chain = CurDAG->getNode(
          AArch64ISD::RET_FLAG,
          SDL_,
          MVT::Other,
          returns
      );
    }

    CurDAG->setRoot(chain);
  }
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
    const MVT argVT = GetVT(arg.GetType());
    const CallLowering::RetLoc &ret = ci.Return(i);
    SDValue argValue = GetValue(arg);
    if (argVT != ret.VT) {
      argValue = CurDAG->getAnyExtOrTrunc(argValue, SDL_, ret.VT);
    }
    chain = CurDAG->getCopyToReg(chain, SDL_, ret.Reg, argValue, flag);
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
void AArch64ISel::LowerSetSP(SDValue value)
{
  CurDAG->setRoot(CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      AArch64::SP,
      value
  ));
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerSet(const SetInst *inst)
{
  auto value = GetValue(inst->GetValue());
  switch (inst->GetReg()->GetValue()) {
    // Stack pointer.
    case ConstantReg::Kind::SP: {
      return LowerSetSP(value);
    }
    // TLS base.
    case ConstantReg::Kind::FS: {
      Error(inst, "Cannot rewrite tls base");
    }
    // Frame address.
    case ConstantReg::Kind::FRAME_ADDR: {
      Error(inst, "Cannot rewrite frame address");
    }
    // Return address.
    case ConstantReg::Kind::RET_ADDR: {
      Error(inst, "Cannot rewrite return address");
    }
  }
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
