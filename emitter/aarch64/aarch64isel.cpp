// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.


#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/IntrinsicsAArch64.h>
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
AArch64Matcher::AArch64Matcher(
    llvm::AArch64TargetMachine &tm,
    llvm::CodeGenOpt::Level ol,
    llvm::MachineFunction &mf)
  : DAGMatcher(
      tm,
      new llvm::SelectionDAG(tm, ol),
      ol,
      mf.getSubtarget().getTargetLowering(),
      mf.getSubtarget().getInstrInfo()
    )
  , AArch64DAGMatcher(
      tm,
      ol,
      &mf.getSubtarget<llvm::AArch64Subtarget>()
    )
  , tm_(tm)
{
  MF = &mf;
}

// -----------------------------------------------------------------------------
AArch64Matcher::~AArch64Matcher()
{
  delete CurDAG;
}

// -----------------------------------------------------------------------------
AArch64ISel::AArch64ISel(
    llvm::AArch64TargetMachine &tm,
    llvm::TargetLibraryInfo &libInfo,
    const Prog &prog,
    llvm::CodeGenOpt::Level ol,
    bool shared)
  : ISel(ID, prog, libInfo, ol)
  , tm_(tm)
  , trampoline_(nullptr)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
llvm::SDValue AArch64ISel::LoadRegArch(ConstantReg::Kind reg)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &TLI = *MF.getSubtarget().getTargetLowering();

  auto mrs = [&, this](const char *code) -> SDValue {
    auto &MRI = MF.getRegInfo();
    auto reg = MRI.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
    auto node = LowerInlineAsm(
        ISD::INLINEASM,
        DAG.getRoot(),
        code,
        0,
        { },
        { },
        { reg }
    );

    auto copy = DAG.getCopyFromReg(
        node.getValue(0),
        SDL_,
        reg,
        MVT::i64,
        node.getValue(1)
    );

    DAG.setRoot(copy.getValue(1));
    return copy.getValue(0);
  };

  switch (reg) {
    case ConstantReg::Kind::FS: return mrs("mrs $0, tpidr_el0");
    case ConstantReg::Kind::AARCH64_FPSR: return mrs("mrs $0, fpsr");
    case ConstantReg::Kind::AARCH64_FPCR: return mrs("mrs $0, fpcr");
    default: {
      llvm_unreachable("invalid aarch64 register");
    }
  }
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerArch(const Inst *inst)
{
  switch (inst->GetKind()) {
    default: {
      llvm_unreachable("invalid architecture-specific instruction");
      return;
    }
    case Inst::Kind::AARCH64_LL: return LowerLL(static_cast<const AArch64_LLInst *>(inst));
    case Inst::Kind::AARCH64_SC: return LowerSC(static_cast<const AArch64_SCInst *>(inst));
    case Inst::Kind::AARCH64_DMB: return LowerDMB(static_cast<const AArch64_DMBInst *>(inst));
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue AArch64ISel::LowerCallee(ConstRef<Inst> inst)
{
  auto &DAG = GetDAG();
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
              return DAG.getTargetGlobalAddress(
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
static std::vector<llvm::Register> kPLTRegs = { AArch64::X16, AArch64::X17 };

// -----------------------------------------------------------------------------
void AArch64ISel::LowerCallSite(SDValue chain, const CallSite *call)
{
  const Block *block = call->getParent();
  const Func *func = block->getParent();

  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  const auto &STI = MF.getSubtarget<llvm::AArch64Subtarget>();
  const auto &TRI = *STI.getRegisterInfo();
  const auto &TLI = *STI.getTargetLowering();
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();
  auto &RVFI = *MF.getInfo<llvm::AArch64FunctionInfo>();
  auto ptrVT = TLI.getPointerTy(DAG.getDataLayout());

  // Analyse the arguments, finding registers for them.
  bool isVarArg = call->IsVarArg();
  bool isTailCall = call->Is(Inst::Kind::TAIL_CALL);
  bool isInvoke = call->Is(Inst::Kind::INVOKE);
  bool isGCCall = call->GetCallingConv() == CallingConv::CAML_GC;
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

  // Find the calling convention and create a mutable copy of the register mask.
  auto [needsTrampoline, cc] = GetCallingConv(func, call);
  const uint32_t *callMask = TRI.getCallPreservedMask(MF, cc);
  uint32_t *mask = MF.allocateRegMask();
  unsigned maskSize = llvm::MachineOperand::getRegMaskSize(TRI.getNumRegs());
  memcpy(mask, callMask, sizeof(mask[0]) * maskSize);

  // Instruction bundle starting the call.
  if (needsAdjust) {
    chain = DAG.getCALLSEQ_START(chain, stackSize, 0, SDL_);
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
          case CallLowering::ArgPart::Kind::BYVAL: {
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
  SDValue callee;
  bool hasStub;
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
    callee = DAG.getTargetGlobalAddress(
        trampoline_,
        SDL_,
        MVT::i64,
        0,
        llvm::AArch64II::MO_NO_FLAG
    );
    hasStub = shared_;
  } else {
    callee = LowerCallee(call->GetCallee());
    if (::cast_or_null<Func>(call->GetCallee())) {
      hasStub = false;
    } else {
      hasStub = shared_ || !isGCCall;
    }
  }

  if (hasStub) {
    for (llvm::Register reg : kPLTRegs) {
      for (llvm::MCSubRegIterator SR(reg, &TRI, true); SR.isValid(); ++SR) {
        mask[*SR / 32] &= ~(1u << (*SR % 32));
      }
    }
  }

  // Prepare arguments in registers.
  SDValue inFlag;
  for (const auto &reg : regArgs) {
    chain = DAG.getCopyToReg(
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
    chain = DAG.getCALLSEQ_END(
        chain,
        DAG.getIntPtrConstant(stackSize, SDL_, true),
        DAG.getIntPtrConstant(0, SDL_, true),
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
    ops.push_back(DAG.getTargetConstant(fpDiff, SDL_, MVT::i32));
  }
  for (const auto &reg : regArgs) {
    ops.push_back(DAG.getRegister(
        reg.first,
        reg.second.getValueType()
    ));
  }
  ops.push_back(DAG.getRegisterMask(mask));

  // Finalize the call node.
  if (inFlag.getNode()) {
    ops.push_back(inFlag);
  }

  // Generate a call or a tail call.
  SDVTList nodeTypes = DAG.getVTList(MVT::Other, MVT::Glue);
  if (isTailCall) {
    MF.getFrameInfo().setHasTailCall();
    DAG.setRoot(DAG.getNode(
        AArch64ISD::TC_RETURN,
        SDL_,
        nodeTypes,
        ops
    ));
  } else {
    chain = DAG.getNode(AArch64ISD::CALL, SDL_, nodeTypes, ops);
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
      chain = DAG.getCALLSEQ_END(
          chain,
          DAG.getIntPtrConstant(stackSize, SDL_, true),
          DAG.getIntPtrConstant(0, SDL_, true),
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

      chain = DAG.getNode(
          AArch64ISD::RET_FLAG,
          SDL_,
          MVT::Other,
          ops
      );
    } else {
      for (auto &[inst, val] : values) {
        Export(inst, val);
      }
    }

    DAG.setRoot(chain);
  }
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerSyscall(const SyscallInst *inst)
{
  auto &DAG = GetDAG();

  static unsigned kRegs[] = {
      AArch64::X0, AArch64::X1, AArch64::X2,
      AArch64::X3, AArch64::X4, AArch64::X5
  };

  llvm::SmallVector<SDValue, 7> ops;
  SDValue chain = DAG.getRoot();

  // Lower the SVC interrupt number.
  ops.push_back(DAG.getTargetConstant(0, SDL_, MVT::i32));

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
      ops.push_back(DAG.getRegister(kRegs[args], MVT::i64));
      chain = DAG.getCopyToReg(chain, SDL_, kRegs[args++], value);
    }
  }

  /// Lower to the syscall.
  {
    ops.push_back(DAG.getRegister(AArch64::X8, MVT::i64));

    chain = DAG.getCopyToReg(
        chain,
        SDL_,
        AArch64::X8,
        GetValue(inst->GetSyscall())
    );

    ops.push_back(chain);

    chain = SDValue(DAG.getMachineNode(
        AArch64::SVC,
        SDL_,
        DAG.getVTList(MVT::Other, MVT::Glue),
        ops
    ), 0);
  }

  /// Copy the return value into a vreg and export it.
  {
    if (auto type = inst->GetType()) {
      if (*type != Type::I64) {
        Error(inst, "invalid syscall type");
      }

      chain = DAG.getCopyFromReg(
          chain,
          SDL_,
          AArch64::X0,
          MVT::i64,
          chain.getValue(1)
      ).getValue(1);

      Export(inst, chain.getValue(0));
    }
  }

  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerClone(const CloneInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &TLI = *MF.getSubtarget().getTargetLowering();

  // Copy in the new stack pointer and code pointer.
  SDValue chain;
  unsigned callee = MRI.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  chain = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      callee,
      GetValue(inst->GetCallee()),
      chain
  );
  unsigned arg = MRI.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  chain = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      arg,
      GetValue(inst->GetArg()),
      chain
  );

  // Copy in other registers.
  auto CopyReg = [&](ConstRef<Inst> arg, unsigned reg) {
    chain = DAG.getCopyToReg(
        DAG.getRoot(),
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
      ISD::INLINEASM,
      chain,
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

    chain = DAG.getCopyFromReg(
        chain,
        SDL_,
        AArch64::X0,
        MVT::i64,
        chain.getValue(1)
    ).getValue(1);

    Export(inst, chain.getValue(0));
  }

  // Update the root.
  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerReturn(const ReturnInst *retInst)
{
  auto &DAG = GetDAG();

  llvm::SmallVector<SDValue, 6> ops;
  ops.push_back(SDValue());

  AArch64Call ci(retInst);
  auto [chain, flag] = LowerRets(GetExportRoot(), ci, retInst, ops);

  ops[0] = chain;
  if (flag.getNode()) {
    ops.push_back(flag);
  }

  DAG.setRoot(DAG.getNode(AArch64ISD::RET_FLAG, SDL_, MVT::Other, ops));
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
void AArch64ISel::LowerLandingPad(const LandingPadInst *inst)
{
  LowerPad(AArch64Call(inst), inst);
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerRaise(const RaiseInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &TLI = *MF.getSubtarget().getTargetLowering();

  // Copy in the new stack pointer and code pointer.
  auto stk = MRI.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  SDValue stkNode = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      stk,
      GetValue(inst->GetStack()),
      SDValue()
  );
  auto pc = MRI.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  SDValue pcNode = DAG.getCopyToReg(
      stkNode,
      SDL_,
      pc,
      GetValue(inst->GetTarget()),
      stkNode.getValue(1)
  );

  // Lower the values to return.
  SDValue glue = pcNode.getValue(1);
  SDValue chain = DAG.getRoot();
  llvm::SmallVector<llvm::Register, 4> regs{ stk, pc };
  if (auto cc = inst->GetCallingConv()) {
    std::tie(chain, glue) = LowerRaises(
        chain,
        AArch64Call(inst),
        inst,
        regs,
        glue
    );
  } else {
    if (!inst->arg_empty()) {
      Error(inst, "missing calling convention");
    }
  }

  DAG.setRoot(LowerInlineAsm(
      ISD::INLINEASM_BR,
      chain,
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
  auto &DAG = GetDAG();
  DAG.setRoot(DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      AArch64::SP,
      value
  ));
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerSet(const SetInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  const auto &TLI = *MF.getSubtarget().getTargetLowering();

  auto value = GetValue(inst->GetValue());

  auto msr = [&, this](const char *code) {
    auto &MRI = MF.getRegInfo();

    auto reg = MRI.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
    SDValue fsNode = DAG.getCopyToReg(
        DAG.getRoot(),
        SDL_,
        reg,
        value,
        SDValue()
    );

    DAG.setRoot(LowerInlineAsm(
        ISD::INLINEASM,
        fsNode.getValue(0),
        code,
        0,
        { reg },
        { },
        { },
        fsNode.getValue(1)
    ));
  };

  switch (inst->GetReg()) {
    case ConstantReg::Kind::SP: return LowerSetSP(value);
    case ConstantReg::Kind::FS: return msr("msr tpidr_el0, $0");
    case ConstantReg::Kind::AARCH64_FPCR: return msr("msr fpcr, $0");
    case ConstantReg::Kind::AARCH64_FPSR: return msr("msr fpsr, $0");
    // Other architecture, ignore.
    case ConstantReg::Kind::RISCV_FFLAGS:
    case ConstantReg::Kind::RISCV_FRM:
    case ConstantReg::Kind::RISCV_FCSR:
    case ConstantReg::Kind::PPC_FPSCR: {
      Error(inst, "Invalid register");
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
void AArch64ISel::LowerLL(const AArch64_LLInst *inst)
{
  auto &DAG = GetDAG();

  SDValue addr = GetValue(inst->GetAddr());

  auto type = inst->GetType();
  MVT retTy = GetVT(type);

  SDValue node = DAG.getMemIntrinsicNode(
      ISD::INTRINSIC_W_CHAIN,
      SDL_,
      DAG.getVTList({ MVT::i64, MVT::Other }),
      {
        DAG.getRoot(),
        DAG.getIntPtrConstant(llvm::Intrinsic::aarch64_ldaxr, SDL_, true),
        addr
      },
      retTy,
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      GetAlignment(type),
      llvm::MachineMemOperand::MOLoad,
      GetSize(type)
  );
  DAG.setRoot(node.getValue(1));
  if (retTy == MVT::i64) {
    Export(inst, node.getValue(0));
  } else {
    Export(inst, DAG.getAnyExtOrTrunc(node.getValue(0), SDL_, retTy));
  }
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerSC(const AArch64_SCInst *inst)
{
  auto &DAG = GetDAG();

  ConstRef<Inst> valArg = inst->GetValue();
  SDValue addr = GetValue(inst->GetAddr());
  SDValue val = GetValue(valArg);

  auto type = valArg.GetType();
  SDValue node = DAG.getMemIntrinsicNode(
      ISD::INTRINSIC_W_CHAIN,
      SDL_,
      DAG.getVTList({ MVT::i32, MVT::Other }),
      {
        DAG.getRoot(),
        DAG.getIntPtrConstant(llvm::Intrinsic::aarch64_stlxr, SDL_, true),
        DAG.getAnyExtOrTrunc(val, SDL_, MVT::i64),
        addr
      },
      GetVT(type),
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      GetAlignment(type),
      llvm::MachineMemOperand::MOStore,
      GetSize(type)
  );
  DAG.setRoot(node.getValue(1));
  Export(inst, node.getValue(0));
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerDMB(const AArch64_DMBInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(DAG.getNode(
      ISD::INTRINSIC_VOID,
      SDL_,
      MVT::Other,
      {
        DAG.getRoot(),
        DAG.getIntPtrConstant(llvm::Intrinsic::aarch64_dmb, SDL_, true),
        DAG.getConstant(0xB, SDL_, MVT::i32)
      }
  ));
}

// -----------------------------------------------------------------------------
void AArch64ISel::LowerVASetup(const AArch64Call &ci)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MFI = MF.getFrameInfo();
  auto &STI = MF.getSubtarget<llvm::AArch64Subtarget>();
  auto &AAFI = *MF.getInfo<llvm::AArch64FunctionInfo>();
  bool isWin64 = STI.isCallingConvWin64(MF.getFunction().getCallingConv());

  if (!STI.isTargetDarwin() || isWin64) {
    SaveVarArgRegisters(ci, isWin64);
  }

  // Set the index to the vararg object.
  unsigned offset = ci.GetFrameSize();
  offset = llvm::alignTo(offset, STI.isTargetILP32() ? 4 : 8);
  AAFI.setVarArgsStackIndex(MFI.CreateFixedObject(4, offset, true));

  if (MFI.hasMustTailInVarArgFunc()) {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
void AArch64ISel::SaveVarArgRegisters(const AArch64Call &ci, bool isWin64)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MFI = MF.getFrameInfo();
  auto &STI = MF.getSubtarget<llvm::AArch64Subtarget>();
  auto &AAFI = *MF.getInfo<llvm::AArch64FunctionInfo>();
  auto &TLI = *MF.getSubtarget().getTargetLowering();
  auto ptrVT = TLI.getPointerTy(DAG.getDataLayout());

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

    SDValue fidx = DAG.getFrameIndex(gprIdx, ptrVT);
    unsigned usedGPRs = ci.GetUsedGPRs().size();
    for (unsigned i = 0; i < unusedGPRs.size(); ++i) {
      unsigned vreg = MF.addLiveIn(unusedGPRs[i], &AArch64::GPR64RegClass);
      SDValue val = DAG.getCopyFromReg(
          DAG.getRoot(),
          SDL_,
          vreg,
          MVT::i64
      );

      llvm::MachinePointerInfo MPI;
      if (isWin64) {
        MPI = llvm::MachinePointerInfo::getFixedStack(MF, gprIdx, i * 8);
      } else {
        MPI = llvm::MachinePointerInfo::getStack(MF, (usedGPRs + i) * 8);
      }

      memOps.push_back(DAG.getStore(val.getValue(1), SDL_, val, fidx, MPI));
      fidx = DAG.getNode(
          ISD::ADD,
          SDL_,
          ptrVT,
          fidx,
          DAG.getConstant(8, SDL_, ptrVT)
      );
    }
  }
  AAFI.setVarArgsGPRIndex(gprIdx);
  AAFI.setVarArgsGPRSize(gprSize);

  if (STI.hasFPARMv8() && !isWin64) {
    auto unusedFPRs = ci.GetUnusedFPRs();
    unsigned fprSize = 16 * unusedFPRs.size();
    int fprIdx = 0;
    if (fprSize != 0) {
      fprIdx = MFI.CreateStackObject(fprSize, llvm::Align(16), false);

      SDValue fidx = DAG.getFrameIndex(fprIdx, ptrVT);
      unsigned usedFPRs = ci.GetUsedFPRs().size();
      for (unsigned i = 0; i < unusedFPRs.size(); ++i) {
        unsigned vreg = MF.addLiveIn(unusedFPRs[i], &AArch64::FPR128RegClass);
        SDValue val = DAG.getCopyFromReg(
            DAG.getRoot(),
            SDL_,
            vreg,
            MVT::f128
        );
        memOps.push_back(DAG.getStore(
            val.getValue(1),
            SDL_,
            val,
            fidx,
            llvm::MachinePointerInfo::getStack(
                DAG.getMachineFunction(),
                (usedFPRs + i) * 16
            )
        ));

        fidx = DAG.getNode(
            ISD::ADD,
            SDL_,
            ptrVT,
            fidx,
            DAG.getConstant(16, SDL_, ptrVT)
        );
      }
    }
    AAFI.setVarArgsFPRIndex(fprIdx);
    AAFI.setVarArgsFPRSize(fprSize);
  }

  if (!memOps.empty()) {
    DAG.setRoot(DAG.getNode(
        ISD::TokenFactor,
        SDL_,
        MVT::Other,
        memOps
    ));
  }
}
