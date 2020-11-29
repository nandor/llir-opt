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
RISCVMatcher::RISCVMatcher(
    llvm::RISCVTargetMachine &tm,
    llvm::CodeGenOpt::Level ol,
    llvm::MachineFunction &mf)
  : DAGMatcher(
      tm,
      new llvm::SelectionDAG(tm, ol),
      ol,
      mf.getSubtarget().getTargetLowering(),
      mf.getSubtarget().getInstrInfo()
    )
  , RISCVDAGMatcher(
      tm,
      ol,
      &mf.getSubtarget<llvm::RISCVSubtarget>()
    )
  , tm_(tm)
{
  MF = &mf;
}

// -----------------------------------------------------------------------------
RISCVMatcher::~RISCVMatcher()
{
  delete CurDAG;
}

// -----------------------------------------------------------------------------
RISCVISel::RISCVISel(
    llvm::RISCVTargetMachine &tm,
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
llvm::SDValue RISCVISel::LoadRegArch(ConstantReg::Kind reg)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  const auto &TLI = *MF.getSubtarget().getTargetLowering();

  auto load = [&, this](const char *code) -> SDValue {
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
    case ConstantReg::Kind::FS: {
      auto copy = DAG.getCopyFromReg(
          DAG.getRoot(),
          SDL_,
          RISCV::X4,
          MVT::i64
      );
      DAG.setRoot(copy.getValue(1));
      return copy.getValue(0);
    }
    case ConstantReg::Kind::RISCV_FFLAGS: return load("frflags $0");
    case ConstantReg::Kind::RISCV_FRM: return load("frrm $0");
    case ConstantReg::Kind::RISCV_FCSR: return load("frcsr $0");
    default: {
      llvm_unreachable("invalid register");
    }
  }
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerArch(const Inst *inst)
{
  switch (inst->GetKind()) {
    default: {
      llvm_unreachable("invalid architecture-specific instruction");
      return;
    }
    case Inst::Kind::RISCV_XCHG:    return LowerXchg(static_cast<const RISCV_XchgInst *>(inst));
    case Inst::Kind::RISCV_CMPXCHG: return LowerCmpXchg(static_cast<const RISCV_CmpXchgInst *>(inst));
    case Inst::Kind::RISCV_FENCE:   return LowerFence(static_cast<const RISCV_FenceInst *>(inst));
    case Inst::Kind::RISCV_GP:      return LowerGP(static_cast<const RISCV_GPInst *>(inst));
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue RISCVISel::LowerCallee(ConstRef<Inst> inst)
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
          case Global::Kind::ATOM: {
            auto name = movGlobal.getName();
            if (auto *GV = M_->getNamedValue(name)) {
              return DAG.getTargetGlobalAddress(
                  GV,
                  SDL_,
                  MVT::i64,
                  0,
                  llvm::RISCVII::MO_CALL
              );
            } else {
              Error(inst.Get(), "Unknown symbol '" + std::string(name) + "'");
            }
          }
          case Global::Kind::EXTERN: {
            auto name = movGlobal.getName();
            if (auto *GV = M_->getNamedValue(name)) {
              return DAG.getTargetGlobalAddress(
                  GV,
                  SDL_,
                  MVT::i64,
                  0,
                  llvm::RISCVII::MO_PLT
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
static std::vector<llvm::Register> kPLTRegs = {
    RISCV::X5, RISCV::X6, RISCV::X7,
    RISCV::X28, RISCV::X29, RISCV::X30, RISCV::X31
};

// -----------------------------------------------------------------------------
void RISCVISel::LowerCallSite(SDValue chain, const CallSite *call)
{
  const Block *block = call->getParent();
  const Func *func = block->getParent();

  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  const auto &STI = MF.getSubtarget<llvm::RISCVSubtarget>();
  const auto &TRI = *STI.getRegisterInfo();
  const auto &TLI = *STI.getTargetLowering();
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();
  auto &RVFI = *MF.getInfo<llvm::RISCVMachineFunctionInfo>();
  auto ptrTy = TLI.getPointerTy(DAG.getDataLayout());

  // Analyse the arguments, finding registers for them.
  bool isVarArg = call->IsVarArg();
  bool isTailCall = call->Is(Inst::Kind::TCALL);
  bool isInvoke = call->Is(Inst::Kind::INVOKE);
  bool isGCCall = call->GetCallingConv() == CallingConv::CAML_GC;
  bool wasTailCall = isTailCall;
  RISCVCall locs(call);

  // Find the number of bytes allocated to hold arguments.
  unsigned stackSize = locs.GetFrameSize();

  // Compute the stack difference for tail calls.
  int fpDiff = 0;
  if (isTailCall) {
    RISCVCall callee(func);
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
    regArgs.emplace_back(RISCV::X7, GetValue(call->GetCallee()));
    callee = DAG.getTargetGlobalAddress(
        trampoline_,
        SDL_,
        MVT::i64,
        0,
        shared_ ? llvm::RISCVII::MO_PLT : llvm::RISCVII::MO_CALL
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
  for (const auto &reg : regArgs) {
    ops.push_back(DAG.getRegister(
        reg.first,
        reg.second.getValueType()
    ));
  }
  if (!isTailCall) {
    ops.push_back(DAG.getRegisterMask(mask));
  }

  // Finalize the call node.
  if (inFlag.getNode()) {
    ops.push_back(inFlag);
  }

  // Generate a call or a tail call.
  SDVTList nodeTypes = DAG.getVTList(MVT::Other, MVT::Glue);
  if (isTailCall) {
    MF.getFrameInfo().setHasTailCall();
    DAG.setRoot(DAG.getNode(
        RISCVISD::TAIL,
        SDL_,
        nodeTypes,
        ops
    ));
  } else {
    chain = DAG.getNode(RISCVISD::CALL, SDL_, nodeTypes, ops);
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
          RISCVISD::RET_FLAG,
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
void RISCVISel::LowerSyscall(const SyscallInst *inst)
{
  auto &DAG = GetDAG();

  static unsigned kRegs[] = {
      RISCV::X10, RISCV::X11, RISCV::X12,
      RISCV::X13, RISCV::X14, RISCV::X15
  };

  llvm::SmallVector<SDValue, 7> ops;
  SDValue chain = DAG.getRoot();

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
    ops.push_back(DAG.getRegister(RISCV::X17, MVT::i64));

    chain = DAG.getCopyToReg(
        chain,
        SDL_,
        RISCV::X17,
        GetValue(inst->GetSyscall())
    );

    ops.push_back(chain);

    chain = SDValue(DAG.getMachineNode(
        RISCV::ECALL,
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
          RISCV::X10,
          MVT::i64,
          chain.getValue(1)
      ).getValue(1);

      Export(inst, chain.getValue(0));
    }
  }

  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerClone(const CloneInst *inst)
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

  CopyReg(inst->GetFlags(), RISCV::X10);
  CopyReg(inst->GetStack(), RISCV::X11);
  CopyReg(inst->GetPTID(), RISCV::X12);
  CopyReg(inst->GetTLS(), RISCV::X13);
  CopyReg(inst->GetCTID(), RISCV::X14);

  chain = LowerInlineAsm(
      ISD::INLINEASM,
      chain,
      "addi x11, x11, -16\n"
      "sd $1, 0(x11)\n"
      "sd $2, 8(x11)\n"
      "li x17, 220\n"
      "ecall\n"
      "bnez x10, 1f\n"
      "ld x11, 0(sp)\n"
      "ld x10, 8(sp)\n"
      "jalr x11\n"
      "li x17, 93\n"
      "ecall\n"
      "1:\n",
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      {
          callee, arg,
          RISCV::X10, RISCV::X11, RISCV::X12, RISCV::X13, RISCV::X14
      },
      { },
      { RISCV::X10 },
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
        RISCV::X10,
        MVT::i64,
        chain.getValue(1)
    ).getValue(1);

    Export(inst, chain.getValue(0));
  }

  // Update the root.
  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerReturn(const ReturnInst *retInst)
{
  auto &DAG = GetDAG();

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
          argValue = DAG.getAnyExtOrTrunc(fullValue, SDL_, part.VT);
        } else {
          argValue = fullValue;
        }
      } else {
        argValue = DAG.getNode(
            ISD::EXTRACT_ELEMENT,
            SDL_,
            part.VT,
            fullValue,
            DAG.getConstant(j, SDL_, part.VT)
        );
      }

      chain = DAG.getCopyToReg(chain, SDL_, part.Reg, argValue, flag);
      ops.push_back(DAG.getRegister(part.Reg, part.VT));
      flag = chain.getValue(1);
    }
  }

  ops[0] = chain;
  if (flag.getNode()) {
    ops.push_back(flag);
  }

  DAG.setRoot(DAG.getNode(
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
void RISCVISel::LowerLandingPad(const LandingPadInst *inst)
{
  LowerPad(RISCVCall(inst), inst);
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerRaise(const RaiseInst *inst)
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
    RISCVCall ci(inst);
    for (unsigned i = 0, n = inst->arg_size(); i < n; ++i) {
      ConstRef<Inst> arg = inst->arg(i);
      SDValue fullValue = GetValue(arg);
      const MVT argVT = GetVT(arg.GetType());
      const CallLowering::RetLoc &ret = ci.Return(i);
      for (unsigned j = 0, m = ret.Parts.size(); j < m; ++j) {
        auto &part = ret.Parts[j];

        SDValue argValue;
        if (m == 1) {
          if (argVT != part.VT) {
            argValue = DAG.getAnyExtOrTrunc(fullValue, SDL_, part.VT);
          } else {
            argValue = fullValue;
          }
        } else {
          argValue = DAG.getNode(
              ISD::EXTRACT_ELEMENT,
              SDL_,
              part.VT,
              fullValue,
              DAG.getConstant(j, SDL_, part.VT)
          );
        }

        chain = DAG.getCopyToReg(chain, SDL_, part.Reg, argValue, glue);
        regs.push_back(part.Reg);
        glue = chain.getValue(1);
      }
    }
  } else {
    if (!inst->arg_empty()) {
      Error(inst, "missing calling convention");
    }
  }

  DAG.setRoot(LowerInlineAsm(
      ISD::INLINEASM_BR,
      chain,
      "mv sp, $0\n"
      "jr $1",
      0,
      regs,
      { },
      { },
      glue
  ));
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerSet(const SetInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &TLI = *MF.getSubtarget().getTargetLowering();

  auto value = GetValue(inst->GetValue());
  auto set = [&, this](const char *code) {
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

  switch (inst->GetReg()->GetValue()) {
    case ConstantReg::Kind::SP: {
      DAG.setRoot(DAG.getCopyToReg(
          DAG.getRoot(),
          SDL_,
          RISCV::X2,
          value
      ));
      return;
    }
    case ConstantReg::Kind::FS: {
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
          "mv tp, $0",
          0,
          { reg },
          { },
          { },
          fsNode.getValue(1)
      ));
      return;
    }
    case ConstantReg::Kind::RISCV_FFLAGS: return set("fscsr $0");
    case ConstantReg::Kind::RISCV_FRM: return set("fsrm $0");
    case ConstantReg::Kind::RISCV_FCSR: return set("fscsr $0");
    // Invalid registers.
    case ConstantReg::Kind::AARCH64_FPCR:
    case ConstantReg::Kind::AARCH64_FPSR:
    case ConstantReg::Kind::PPC_FPSCR: {
      llvm_unreachable("invalid register");
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
  llvm_unreachable("invalid register kind");
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerXchg(const RISCV_XchgInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  auto *mmo = dag.getMachineFunction().getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile |
      llvm::MachineMemOperand::MOLoad |
      llvm::MachineMemOperand::MOStore,
      GetSize(inst->GetType()),
      llvm::Align(GetSize(inst->GetType())),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDValue xchg = dag.getAtomic(
      ISD::ATOMIC_SWAP,
      SDL_,
      GetVT(inst->GetType()),
      dag.getRoot(),
      GetValue(inst->GetAddr()),
      GetValue(inst->GetVal()),
      mmo
  );

  dag.setRoot(xchg.getValue(1));
  Export(inst, xchg.getValue(0));
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerCmpXchg(const RISCV_CmpXchgInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();

  auto type = inst->GetType();
  size_t size = GetSize(type);
  MVT retTy = GetVT(type);

  auto *mmo = MF.getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile |
      llvm::MachineMemOperand::MOLoad |
      llvm::MachineMemOperand::MOStore,
      size,
      llvm::Align(size),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDVTList VTs = DAG.getVTList(retTy, MVT::i1, MVT::Other);
  SDValue Swap = DAG.getAtomicCmpSwap(
      ISD::ATOMIC_CMP_SWAP_WITH_SUCCESS,
      SDL_,
      retTy,
      VTs,
      DAG.getRoot(),
      GetValue(inst->GetAddr()),
      GetValue(inst->GetRef()),
      GetValue(inst->GetVal()),
      mmo
  );
  Export(inst, Swap.getValue(0));
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerFence(const RISCV_FenceInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(LowerInlineAsm(
        ISD::INLINEASM,
        DAG.getRoot(),
        "fence rw, rw",
        0,
        { },
        { },
        { }
  ));
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerGP(const RISCV_GPInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(LowerInlineAsm(
        ISD::INLINEASM,
        DAG.getRoot(),
        ".weak __global_pointer$$\n"
        ".hidden __global_pointer$$\n"
        ".option push\n"
        ".option norelax\n\t"
        "lla gp, __global_pointer$$\n"
        ".option pop\n\t",
        0,
        { },
        { },
        { }
  ));
}

// -----------------------------------------------------------------------------
void RISCVISel::LowerVASetup(const RISCVCall &ci)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  const llvm::TargetRegisterClass *RC = &RISCV::GPRRegClass;
  auto &MFI = MF.getFrameInfo();
  auto &MRI = MF.getRegInfo();
  auto &RVFI = *MF.getInfo<llvm::RISCVMachineFunctionInfo>();
  const auto &STI = MF.getSubtarget<llvm::RISCVSubtarget>();

  // Find unused registers.
  MVT xLenVT = STI.getXLenVT();
  unsigned xLen = STI.getXLen() / 8;
  auto unusedRegs = ci.GetUnusedGPRs();

  // Find the size & offset of the vararg save area.
  int vaSize = xLen * unusedRegs.size();
  int vaOffset = -vaSize;
  RVFI.setVarArgsFrameIndex(MFI.CreateFixedObject(xLen, vaOffset, true));

  // Pad to alignment.
  if (unusedRegs.size() % 2) {
    MFI.CreateFixedObject(xLen, vaOffset - (int)xLen, true);
    vaSize += xLen;
  }
  RVFI.setVarArgsSaveSize(vaSize);

  // Copy registers to the save area.
  SDValue chain = DAG.getRoot();
  llvm::SmallVector<SDValue, 8> stores;
  for (llvm::Register unusedReg : unusedRegs) {
    const llvm::Register reg = MRI.createVirtualRegister(RC);
    MRI.addLiveIn(unusedReg, reg);

    int fi = MFI.CreateFixedObject(xLen, vaOffset, true);
    SDValue arg = DAG.getCopyFromReg(chain, SDL_, reg, xLenVT);
    SDValue store = DAG.getStore(
        chain,
        SDL_,
        arg,
        DAG.getFrameIndex(fi, GetPtrTy()),
        llvm::MachinePointerInfo::getFixedStack(MF, fi)
    );

    auto *mo = llvm::cast<llvm::StoreSDNode>(store.getNode())->getMemOperand();
    mo->setValue(static_cast<llvm::Value *>(nullptr));

    stores.push_back(store);
    vaOffset += xLen;
  }

  if (!stores.empty()) {
    stores.push_back(chain);
    DAG.setRoot(DAG.getNode(ISD::TokenFactor, SDL_, MVT::Other, stores));
  }
}
