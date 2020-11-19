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
#include <llvm/Target/PowerPC/PPC.h>
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
    const llvm::PPCTargetLowering *TLI,
    llvm::TargetLibraryInfo *LibInfo,
    const Prog &prog,
    llvm::CodeGenOpt::Level OL,
    bool shared)
  : DAGMatcher(*TM, new llvm::SelectionDAG(*TM, OL), OL, TLI, TII)
  , PPCDAGMatcher(*TM, OL, TLI, STI)
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
  switch (reg) {
    default: {
      llvm_unreachable("invalid ppc register");
    }
    case ConstantReg::Kind::FS: {
      auto copy = CurDAG->getCopyFromReg(
          CurDAG->getRoot(),
          SDL_,
          PPC::X13,
          MVT::i64
      );
      CurDAG->setRoot(copy.getValue(1));
      return copy.getValue(0);
    }
    case ConstantReg::Kind::PPC_FPSCR: {
      auto &RegInfo = MF->getRegInfo();
      auto reg = RegInfo.createVirtualRegister(TLI->getRegClassFor(MVT::f64));
      auto node = LowerInlineAsm(
          ISD::INLINEASM,
          CurDAG->getRoot(),
          "mffs $0",
          0,
          { },
          { },
          { reg }
      );

      auto copy = CurDAG->getCopyFromReg(
          node.getValue(0),
          SDL_,
          reg,
          MVT::f64,
          node.getValue(1)
      );

      CurDAG->setRoot(copy.getValue(1));
      return copy.getValue(0);
    }
  }
}

// -----------------------------------------------------------------------------
void PPCISel::LowerArch(const Inst *inst)
{
  switch (inst->GetKind()) {
    default: {
      llvm_unreachable("invalid architecture-specific instruction");
      return;
    }
    case Inst::Kind::PPC_LL: return LowerLL(static_cast<const PPC_LLInst *>(inst));
    case Inst::Kind::PPC_SC: return LowerSC(static_cast<const PPC_SCInst *>(inst));
    case Inst::Kind::PPC_SYNC: return LowerSync(static_cast<const PPC_SyncInst *>(inst));
    case Inst::Kind::PPC_ISYNC: return LowerISync(static_cast<const PPC_ISyncInst *>(inst));
  }
}

// -----------------------------------------------------------------------------
std::pair<unsigned, llvm::SDValue> PPCISel::LowerCallee(ConstRef<Inst> inst)
{
  if (ConstRef<MovInst> movInst = ::cast_or_null<MovInst>(inst)) {
    ConstRef<Value> movArg = GetMoveArg(movInst.Get());
    switch (movArg->GetKind()) {
      case Value::Kind::INST: {
        auto argInst = ::cast<Inst>(movArg);
        if (STI_->isUsingPCRelativeCalls()) {
          return std::make_pair(PPCISD::BCTRL, GetValue(argInst));
        } else {
          return std::make_pair(PPCISD::BCTRL_LOAD_TOC, GetValue(argInst));
        }
      }
      case Value::Kind::GLOBAL: {
        const Global &movGlobal = *::cast<Global>(movArg);
        switch (movGlobal.GetKind()) {
          case Global::Kind::BLOCK:
          case Global::Kind::ATOM: {
            llvm_unreachable("invalid call argument");
          }
          case Global::Kind::FUNC: {
            auto name = movGlobal.getName();
            if (auto *GV = M_->getNamedValue(name)) {
              return std::make_pair(
                  PPCISD::CALL,
                  CurDAG->getTargetGlobalAddress(
                      GV,
                      SDL_,
                      MVT::i64,
                      0,
                      llvm::PPCII::MO_NO_FLAG
                  )
              );
            } else {
              Error(inst.Get(), "Unknown symbol '" + std::string(name) + "'");
            }
          }
          case Global::Kind::EXTERN: {
            auto name = movGlobal.getName();
            if (auto *GV = M_->getNamedValue(name)) {
              return std::make_pair(
                  PPCISD::CALL_NOP,
                  CurDAG->getTargetGlobalAddress(
                      GV,
                      SDL_,
                      MVT::i64,
                      0,
                      llvm::PPCII::MO_NO_FLAG
                  )
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
    if (STI_->isUsingPCRelativeCalls()) {
      return std::make_pair(PPCISD::BCTRL, GetValue(inst));
    } else {
      return std::make_pair(PPCISD::BCTRL_LOAD_TOC, GetValue(inst));
    }
  }
}

// -----------------------------------------------------------------------------
void PPCISel::LowerCallSite(SDValue chain, const CallSite *call)
{
  const Block *block = call->getParent();
  const Func *func = block->getParent();
  auto ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();
  auto &TRI = GetRegisterInfo();

  // Analyse the arguments, finding registers for them.
  bool isVarArg = call->IsVarArg();
  bool isTailCall = call->Is(Inst::Kind::TCALL);
  bool isInvoke = call->Is(Inst::Kind::INVOKE);
  bool wasTailCall = isTailCall;
  PPCCall locs(call);

  // Find the number of bytes allocated to hold arguments.
  unsigned stackSize = locs.GetFrameSize();

  // Compute the stack difference for tail calls.
  int fpDiff = 0;
  if (isTailCall) {
    PPCCall callee(func);
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

  // Find the calling convention and create a mutable copy of the register mask.
  auto [needsTrampoline, cc] = GetCallingConv(func, call);
  const uint32_t *callMask = TRI_->getCallPreservedMask(*MF, cc);
  uint32_t *mask = MF->allocateRegMask();
  unsigned maskSize = llvm::MachineOperand::getRegMaskSize(TRI.getNumRegs());
  memcpy(mask, callMask, sizeof(mask[0]) * maskSize);

  // Instruction bundle starting the call.
  if (needsAdjust) {
    chain = CurDAG->getCALLSEQ_START(chain, stackSize, 0, SDL_);
  }

  // Identify registers and stack locations holding the arguments.
  llvm::SmallVector<std::pair<unsigned, SDValue>, 8> regArgs;
  chain = LowerCallArguments(chain, call, locs, regArgs);

  if (isTailCall) {
    // Shuffle arguments on the stack.
    for (auto it = locs.arg_begin(); it != locs.arg_end(); ++it) {
      for (unsigned i = 0, n = it->Parts.size(); i < n; ++i) {
        auto &part = it->Parts[i];
        switch (part.K) {
          case CallLowering::ArgPart::Kind::REG: {
            continue;
          }
          case CallLowering::ArgPart::Kind::STK: {
            llvm_unreachable("not implemented");
            break;
          }
        }
      }
    }

    // Store the return address.
    if (fpDiff) {
      llvm_unreachable("not implemented");
    }
  }

  // Find the callee.
  unsigned opcode;
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
    regArgs.emplace_back(PPC::X25, GetValue(call->GetCallee()));
    opcode = shared_ ? PPCISD::CALL_NOP : PPCISD::CALL;
    callee = CurDAG->getTargetGlobalAddress(
        trampoline_,
        SDL_,
        MVT::i64,
        0,
        llvm::PPCII::MO_NO_FLAG
    );
  } else {
    std::tie(opcode, callee) = LowerCallee(call->GetCallee());
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
  // Add the TOC register as an argument.
  if (!STI_->isUsingPCRelativeCalls()) {
    FuncInfo_->setUsesTOCBasePtr();
    ops.push_back(CurDAG->getRegister(STI_->getTOCPointerRegister(), MVT::i64));
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
        PPCISD::TC_RETURN,
        SDL_,
        nodeTypes,
        ops
    ));
  } else {
    chain = CurDAG->getNode(opcode, SDL_, nodeTypes, ops);
    inFlag = chain.getValue(1);

    // Find the register to store the return value in.
    llvm::SmallVector<CallLowering::RetLoc, 3> returns;
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
      chain = LowerGCFrame(chain, inFlag, call);
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
    llvm::SmallVector<SDValue, 3> regs;
    llvm::SmallVector<std::pair<ConstRef<Inst>, SDValue>, 3> values;
    auto node = LowerReturns(chain, inFlag, call, returns, regs, values);
    chain = node.first;
    inFlag = node.second;

    if (wasTailCall) {
      llvm::SmallVector<SDValue, 6> ops;
      ops.push_back(chain);
      for (auto &reg : regs) {
        ops.push_back(reg);
      }

      chain = CurDAG->getNode(
          PPCISD::RET_FLAG,
          SDL_,
          MVT::Other,
          ops
      );
    } else {
      for (auto &[inst, val] : values) {
        Export(inst, val);
      }
    }

    CurDAG->setRoot(chain);
  }
}

// -----------------------------------------------------------------------------
static llvm::Register kSyscallRegs[] = {
    PPC::X3, PPC::X4, PPC::X5,
    PPC::X6, PPC::X7, PPC::X8
};

// -----------------------------------------------------------------------------
void PPCISel::LowerSyscall(const SyscallInst *inst)
{
  auto &RegInfo = MF->getRegInfo();
  auto &TLI = GetTargetLowering();

  llvm::SmallVector<llvm::Register, 7> ops;
  SDValue chain = CurDAG->getRoot();

  // Lower the syscall number.
  chain = CurDAG->getCopyToReg(
      chain,
      SDL_,
      PPC::X0,
      GetValue(inst->GetSyscall()),
      SDValue()
  );
  ops.push_back(PPC::X0);

  // Lower arguments.
  unsigned args = 0;
  {
    unsigned n = sizeof(kSyscallRegs) / sizeof(kSyscallRegs[0]);
    for (ConstRef<Inst> arg : inst->args()) {
      if (args >= n) {
        Error(inst, "too many arguments to syscall");
      }

      SDValue value = GetValue(arg);
      if (arg.GetType() != Type::I64) {
        Error(inst, "invalid syscall argument");
      }
      ops.push_back(kSyscallRegs[args]);
      chain = CurDAG->getCopyToReg(
          chain.getValue(0),
          SDL_,
          kSyscallRegs[args++],
          value,
          chain.getValue(1)
      );
    }
  }

  // Prepare a reg for the syscall number.
  chain = LowerInlineAsm(
      ISD::INLINEASM,
      chain,
      "sc\n"
      "bns+ 1f\n"
      "neg 3, 3\n"
      "1:\n",
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      ops,
      { },
      { },
      chain.getValue(1)
  );

  {
    if (auto type = inst->GetType()) {
      if (*type != Type::I64) {
        Error(inst, "invalid syscall type");
      }

      chain = CurDAG->getCopyFromReg(
          chain,
          SDL_,
          PPC::X3,
          MVT::i64,
          chain.getValue(1)
      ).getValue(1);

      Export(inst, chain.getValue(0));
    }
  }

  CurDAG->setRoot(chain);
}

// -----------------------------------------------------------------------------
void PPCISel::LowerClone(const CloneInst *inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCISel::LowerReturn(const ReturnInst *retInst)
{
  llvm::SmallVector<SDValue, 6> ops;
  ops.push_back(SDValue());

  SDValue flag;
  SDValue chain = GetExportRoot();

  PPCCall ci(retInst);
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
      PPCISD::RET_FLAG,
      SDL_,
      MVT::Other,
      ops
  ));
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
  auto value = GetValue(inst->GetValue());

  switch (inst->GetReg()->GetValue()) {
    default: {
      Error(inst, "Cannot rewrite register");
    }
    case ConstantReg::Kind::SP: {
       auto &RegInfo = MF->getRegInfo();

      auto reg = RegInfo.createVirtualRegister(TLI->getRegClassFor(MVT::i64));
      SDValue spNode = CurDAG->getCopyToReg(
          CurDAG->getRoot(),
          SDL_,
          reg,
          value,
          SDValue()
      );

      CurDAG->setRoot(LowerInlineAsm(
          ISD::INLINEASM,
          spNode.getValue(0),
          "mr 1, $0",
          0,
          { reg },
          { },
          { },
          spNode.getValue(1)
      ));
      return;
    }
    case ConstantReg::Kind::FS: {
       auto &RegInfo = MF->getRegInfo();

      auto reg = RegInfo.createVirtualRegister(TLI->getRegClassFor(MVT::i64));
      SDValue fsNode = CurDAG->getCopyToReg(
          CurDAG->getRoot(),
          SDL_,
          reg,
          value,
          SDValue()
      );

      CurDAG->setRoot(LowerInlineAsm(
          ISD::INLINEASM,
          fsNode.getValue(0),
          "mr 13, $0",
          0,
          { reg },
          { },
          { },
          fsNode.getValue(1)
      ));
      return;
    }
    case ConstantReg::Kind::PPC_FPSCR: {
      auto &RegInfo = MF->getRegInfo();

      auto reg = RegInfo.createVirtualRegister(TLI->getRegClassFor(MVT::f64));
      SDValue fsNode = CurDAG->getCopyToReg(
          CurDAG->getRoot(),
          SDL_,
          reg,
          value,
          SDValue()
      );

      CurDAG->setRoot(LowerInlineAsm(
          ISD::INLINEASM,
          fsNode.getValue(0),
          "mtfsf 255, $0",
          0,
          { reg },
          { },
          { },
          fsNode.getValue(1)
      ));
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void PPCISel::LowerLL(const PPC_LLInst *inst)
{
  auto &RegInfo = MF->getRegInfo();
  auto &TLI = GetTargetLowering();

  SDValue chain;
  unsigned addr = RegInfo.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  chain = CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      addr,
      GetValue(inst->GetAddr()),
      chain
  );

  unsigned ret = RegInfo.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  switch (inst->GetType()) {
    case Type::I32: {
      chain = LowerInlineAsm(
          ISD::INLINEASM,
          chain,
          "lwarx $1, 0, $0",
          llvm::InlineAsm::Extra_MayLoad,
          { addr },
          { PPC::CR0 },
          { ret },
          chain.getValue(1)
      );
      break;
    }
    case Type::I64: {
      chain = LowerInlineAsm(
          ISD::INLINEASM,
          chain,
          "ldarx $1, 0, $0",
          llvm::InlineAsm::Extra_MayLoad,
          { addr },
          { PPC::CR0 },
          { ret },
          chain.getValue(1)
      );
      break;
    }
    default: {
      llvm_unreachable("invalid load-linked type");
    }
  }

  chain = CurDAG->getCopyFromReg(
      chain,
      SDL_,
      ret,
      MVT::i64,
      chain.getValue(1)
  ).getValue(1);

  Export(inst, CurDAG->getAnyExtOrTrunc(
      chain.getValue(0),
      SDL_,
      GetVT(inst->GetType())
  ));
}

// -----------------------------------------------------------------------------
void PPCISel::LowerSC(const PPC_SCInst *inst)
{
  auto &RegInfo = MF->getRegInfo();
  auto &TLI = GetTargetLowering();

  SDValue chain;
  unsigned addr = RegInfo.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  chain = CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      addr,
      GetValue(inst->GetAddr()),
      chain
  );
  unsigned value = RegInfo.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  chain = CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      value,
      CurDAG->getAnyExtOrTrunc(GetValue(inst->GetValue()), SDL_, MVT::i64),
      chain
  );

  unsigned ret = RegInfo.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  switch (inst->GetValue().GetType()) {
    case Type::I32: {
      chain = LowerInlineAsm(
          ISD::INLINEASM,
          chain,
          "stwcx. $0, 0, $1\n"
          "mfcr $2\n",
          llvm::InlineAsm::Extra_MayLoad,
          { addr, value },
          { PPC::CR0 },
          { ret },
          chain.getValue(1)
      );
      break;
    }
    case Type::I64: {
      chain = LowerInlineAsm(
          ISD::INLINEASM,
          chain,
          "stdcx. $0, 0, $1\n"
          "mfcr $2",
          llvm::InlineAsm::Extra_MayLoad,
          { addr, value },
          { PPC::CR0 },
          { ret },
          chain.getValue(1)
      );
      break;
    }
    default: {
      llvm_unreachable("invalid load-linked type");
    }
  }

  chain = CurDAG->getCopyFromReg(
      chain,
      SDL_,
      ret,
      MVT::i64,
      chain.getValue(1)
  ).getValue(1);

  SDValue flag = CurDAG->getNode(
      ISD::AND,
      SDL_,
      MVT::i64,
      chain.getValue(0),
      CurDAG->getConstant(0x20000000, SDL_, MVT::i64)
  );

  Export(inst, CurDAG->getSetCC(
      SDL_,
      GetVT(inst->GetType()),
      flag,
      CurDAG->getConstant(0, SDL_, MVT::i64),
      ISD::CondCode::SETNE
  ));
}

// -----------------------------------------------------------------------------
void PPCISel::LowerSync(const PPC_SyncInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(DAG.getNode(
      ISD::INTRINSIC_VOID,
      SDL_,
      MVT::Other,
      {
        DAG.getRoot(),
        DAG.getTargetConstant(llvm::Intrinsic::ppc_sync, SDL_, GetPtrTy())
      }
  ));
}

// -----------------------------------------------------------------------------
void PPCISel::LowerISync(const PPC_ISyncInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(DAG.getNode(
      ISD::INTRINSIC_VOID,
      SDL_,
      MVT::Other,
      {
        DAG.getRoot(),
        DAG.getTargetConstant(llvm::Intrinsic::ppc_isync, SDL_, GetPtrTy())
      }
  ));
}

// -----------------------------------------------------------------------------
void PPCISel::LowerVASetup(const PPCCall &ci)
{
  llvm::MVT ptrTy = GetPtrTy();
  auto &DAG = GetDAG();
  auto &MFI = MF->getFrameInfo();
  auto &PFI = *MF->getInfo<llvm::PPCFunctionInfo>();

  PFI.setVarArgsFrameIndex(MFI.CreateFixedObject(8, ci.GetFrameSize(), true));
  SDValue off = DAG.getFrameIndex(PFI.getVarArgsFrameIndex(), ptrTy);

  llvm::SmallVector<SDValue, 8> stores;
  for (llvm::Register unusedReg : ci.GetUnusedGPRs()) {
    llvm::Register reg = MF->addLiveIn(unusedReg, &PPC::G8RCRegClass);
    SDValue val = DAG.getCopyFromReg(DAG.getRoot(), SDL_, reg, ptrTy);
    stores.push_back(DAG.getStore(
        val.getValue(1),
        SDL_,
        val,
        off,
        llvm::MachinePointerInfo()
    ));
    off = DAG.getNode(
        ISD::ADD,
        SDL_,
        ptrTy,
        off,
        DAG.getConstant(8, SDL_, ptrTy)
    );
  }

  if (!stores.empty()) {
    stores.push_back(DAG.getRoot());
    DAG.setRoot(DAG.getNode(ISD::TokenFactor, SDL_, MVT::Other, stores));
  }
}

// -----------------------------------------------------------------------------
llvm::MVT PPCISel::GetFlagTy() const
{
  return STI_->useCRBits() ? MVT::i1 : MVT::i32;
}

// -----------------------------------------------------------------------------
llvm::ScheduleDAGSDNodes *PPCISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}
