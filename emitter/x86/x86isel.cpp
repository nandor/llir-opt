// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <bitset>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/IntrinsicsX86.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/MachineJumpTableInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/X86/X86ISelLowering.h>

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
#include "emitter/x86/x86call.h"
#include "emitter/x86/x86isel.h"

namespace ISD = llvm::ISD;
namespace X86 = llvm::X86;
namespace X86ISD = llvm::X86ISD;
using MVT = llvm::MVT;
using EVT = llvm::EVT;
using GlobalValue = llvm::GlobalValue;
using SDNodeFlags = llvm::SDNodeFlags;
using SDNode = llvm::SDNode;
using SDValue = llvm::SDValue;
using SDVTList = llvm::SDVTList;
using SelectionDAG = llvm::SelectionDAG;
using X86RegisterInfo = llvm::X86RegisterInfo;
using GlobalAddressSDNode = llvm::GlobalAddressSDNode;
using ConstantSDNode = llvm::ConstantSDNode;


// -----------------------------------------------------------------------------
char X86ISel::ID;

// -----------------------------------------------------------------------------
X86Matcher::X86Matcher(
    llvm::X86TargetMachine &tm,
    llvm::CodeGenOpt::Level ol,
    llvm::MachineFunction &mf)
  : DAGMatcher(
      tm,
      new llvm::SelectionDAG(tm, ol),
      ol,
      mf.getSubtarget().getTargetLowering(),
      mf.getSubtarget().getInstrInfo()
    )
  , X86DAGMatcher(tm, ol, &mf.getSubtarget<llvm::X86Subtarget>())
  , tm_(tm)
{
  MF = &mf;
}

// -----------------------------------------------------------------------------
X86Matcher::~X86Matcher()
{
  delete CurDAG;
}

// -----------------------------------------------------------------------------
X86ISel::X86ISel(
    llvm::X86TargetMachine &tm,
    llvm::TargetLibraryInfo &LibInfo,
    const Prog &prog,
    llvm::CodeGenOpt::Level ol,
    bool shared)
  : ISel(ID, prog, LibInfo, ol)
  , tm_(tm)
  , trampoline_(nullptr)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
void X86ISel::LowerArch(const Inst *i)
{
  switch (i->GetKind()) {
    default: {
      llvm_unreachable("invalid architecture-specific instruction");
      return;
    }
    case Inst::Kind::X86_FN_ST_CW:    return LowerFPUControl(X86ISD::FNSTCW16m, 2, true, i);
    case Inst::Kind::X86_FN_ST_SW:    return LowerFPUControl(X86ISD::FNSTSW16m, 2, true, i);
    case Inst::Kind::X86_FN_ST_ENV:   return LowerFPUControl(X86ISD::FNSTENVm, 28, true, i);
    case Inst::Kind::X86_F_LD_CW:     return LowerFPUControl(X86ISD::FLDCW16m, 2, false, i);
    case Inst::Kind::X86_F_LD_ENV:    return LowerFPUControl(X86ISD::FLDENVm, 28, false, i);
    case Inst::Kind::X86_LDM_XCSR:    return LowerFPUControl(X86ISD::LDMXCSR32m, 4, false, i);
    case Inst::Kind::X86_STM_XCSR:    return LowerFPUControl(X86ISD::STMXCSR32m, 4, true, i);
    case Inst::Kind::X86_F_SAVE:      llvm_unreachable("not implemented");
    case Inst::Kind::X86_F_RESTORE:   llvm_unreachable("not implemented");
    case Inst::Kind::X86_F_X_SAVE:    llvm_unreachable("not implemented");
    case Inst::Kind::X86_F_X_RESTORE: llvm_unreachable("not implemented");
    case Inst::Kind::X86_X_SAVE:      llvm_unreachable("not implemented");
    case Inst::Kind::X86_X_SAVE_OPT:  return Lower(static_cast<const X86_XSaveOptInst *>(i));
    case Inst::Kind::X86_X_RESTORE:   return Lower(static_cast<const X86_XRestoreInst *>(i));
    case Inst::Kind::X86_XCHG:        return Lower(static_cast<const X86_XchgInst *>(i));
    case Inst::Kind::X86_CMP_XCHG:    return Lower(static_cast<const X86_CmpXchgInst *>(i));
    case Inst::Kind::X86_FN_CL_EX:    return Lower(static_cast<const X86_FnClExInst *>(i));
    case Inst::Kind::X86_RD_TSC:      return Lower(static_cast<const X86_RdTscInst *>(i));
    case Inst::Kind::X86_M_FENCE:     return Lower(static_cast<const BarrierInst *>(i), X86ISD::MFENCE);
    case Inst::Kind::X86_L_FENCE:     return Lower(static_cast<const BarrierInst *>(i), X86ISD::LFENCE);
    case Inst::Kind::X86_S_FENCE:     return Lower(static_cast<const BarrierInst *>(i), X86ISD::SFENCE);
    case Inst::Kind::X86_BARRIER:     return Lower(static_cast<const BarrierInst *>(i), X86ISD::MEMBARRIER);
    case Inst::Kind::X86_CPU_ID:      return Lower(static_cast<const X86_CpuIdInst *>(i));
    case Inst::Kind::X86_IN:          return Lower(static_cast<const X86_InInst *>(i));
    case Inst::Kind::X86_OUT:         return Lower(static_cast<const X86_OutInst *>(i));
    case Inst::Kind::X86_WR_MSR:      return Lower(static_cast<const X86_WrMsrInst *>(i));
    case Inst::Kind::X86_RD_MSR:      return Lower(static_cast<const X86_RdMsrInst *>(i));
    case Inst::Kind::X86_PAUSE:       return Lower(static_cast<const X86_PauseInst *>(i));
    case Inst::Kind::X86_STI:         return Lower(static_cast<const X86_StiInst *>(i));
    case Inst::Kind::X86_CLI:         return Lower(static_cast<const X86_CliInst *>(i));
    case Inst::Kind::X86_HLT:         return Lower(static_cast<const X86_HltInst *>(i));
    case Inst::Kind::X86_SPIN:        return Lower(static_cast<const X86_SpinInst *>(i));
    case Inst::Kind::X86_LGDT:        return Lower(static_cast<const X86_LgdtInst *>(i));
    case Inst::Kind::X86_LIDT:        return Lower(static_cast<const X86_LidtInst *>(i));
    case Inst::Kind::X86_LTR:         return Lower(static_cast<const X86_LtrInst *>(i));
  }
}

// -----------------------------------------------------------------------------
template <typename T>
std::unique_ptr<X86Call> GetCallLowering(SelectionDAG &DAG, T *inst)
{
  auto &STI = DAG.getMachineFunction().getSubtarget<llvm::X86Subtarget>();
  if (STI.is32Bit()) {
    return std::make_unique<X86_32Call>(inst);
  } else {
    return std::make_unique<X86_64Call>(inst);
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerReturn(const ReturnInst *retInst)
{
  auto &DAG = GetDAG();
  llvm::SmallVector<SDValue, 6> ops;
  ops.push_back(GetExportRoot());
  ops.push_back(DAG.getTargetConstant(0, SDL_, MVT::i32));

  if (func_->GetCallingConv() == CallingConv::INTR) {
    assert(retInst->arg_size() == 0 && "nothing to return");
    DAG.setRoot(DAG.getNode(X86ISD::IRET, SDL_, MVT::Other, ops));
  } else {
    std::unique_ptr<X86Call> ci(GetCallLowering(GetDAG(), retInst));
    auto [chain, flag] = LowerRets(ops[0], *ci, retInst, ops);

    ops[0] = chain;
    if (flag.getNode()) {
      ops.push_back(flag);
    }

    DAG.setRoot(DAG.getNode(X86ISD::RET_FLAG, SDL_, MVT::Other, ops));
  }
}


// -----------------------------------------------------------------------------
void X86ISel::LowerSet(const SetInst *inst)
{
  auto value = GetValue(inst->GetValue());
  auto ptrVT = GetPointerType();
  switch (inst->GetReg()) {
    // Stack pointer.
    case Register::SP: {
      return LowerSetSP(value);
    }
    case Register::X86_CR0: return LowerSetReg("mov $0, %cr0", ptrVT, value);
    case Register::X86_CR2: return LowerSetReg("mov $0, %cr2", ptrVT, value);
    case Register::X86_CR3: return LowerSetReg("mov $0, %cr3", ptrVT, value);
    case Register::X86_CR4: return LowerSetReg("mov $0, %cr4", ptrVT, value);
    case Register::X86_CS:  return LowerSetCs(value);
    case Register::X86_DS:  return LowerSetReg("mov $0, %ds", MVT::i32, value);
    case Register::X86_ES:  return LowerSetReg("mov $0, %es", MVT::i32, value);
    case Register::X86_SS:  return LowerSetReg("mov $0, %ss", MVT::i32, value);
    case Register::X86_FS:  return LowerSetReg("mov $0, %fs", MVT::i32, value);
    case Register::X86_GS:  return LowerSetReg("mov $0, %gs", MVT::i32, value);
    // Architecture-specific registers.
    case Register::FS:
    case Register::FRAME_ADDR:
    case Register::RET_ADDR:
    case Register::AARCH64_FPSR:
    case Register::AARCH64_FPCR:
    case Register::AARCH64_CNTVCT:
    case Register::AARCH64_CNTFRQ:
    case Register::AARCH64_FAR:
    case Register::AARCH64_VBAR:
    case Register::RISCV_FFLAGS:
    case Register::RISCV_FRM:
    case Register::RISCV_FCSR:
    case Register::PPC_FPSCR:  {
      llvm::report_fatal_error("cannot rewrite immutable register");
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSetCs(SDValue value)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &STI = MF.getSubtarget();
  const auto &TLI = *STI.getTargetLowering();

  // Copy in the new stack pointer and code pointer.
  auto addr = MRI.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  SDValue addrNode = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      addr,
      DAG.getAnyExtOrTrunc(value, SDL_, MVT::i64),
      SDValue()
  );

  DAG.setRoot(LowerInlineAsm(
      ISD::INLINEASM,
      addrNode.getValue(0),
      "pushq $0;\n"
      "pushq $$1f;\n"
      "lretq;\n"
      "1:\n",
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      { addr },
      { },
      { },
      addrNode.getValue(1)
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerFPUControl(
    unsigned opcode,
    unsigned bytes,
    bool store,
    const Inst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();

  auto *fpuInst = static_cast<const X86_FPUControlInst *>(inst);

  llvm::MachineMemOperand::Flags flag;
  if (store) {
    flag = llvm::MachineMemOperand::MOStore;
  } else {
    flag = llvm::MachineMemOperand::MOLoad;
  }

  auto *mmo = MF.getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile | flag,
      bytes,
      llvm::Align(1),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDValue addr = GetValue(fpuInst->GetAddr());
  DAG.setRoot(
      DAG.getMemIntrinsicNode(
          opcode,
          SDL_,
          DAG.getVTList(MVT::Other),
          { DAG.getRoot(), addr },
          MVT::i16,
          mmo
      )
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerArguments(bool hasVAStart)
{
  std::unique_ptr<X86Call> ci(GetCallLowering(GetDAG(), func_));
  if (hasVAStart) {
    LowerVASetup(*ci);
  }
  LowerArgs(*ci);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerLandingPad(const LandingPadInst *inst)
{
  std::unique_ptr<X86Call> ci(GetCallLowering(GetDAG(), inst));
  LowerPad(*ci, inst);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerVASetup(const X86Call &ci)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &STI = MF.getSubtarget<llvm::X86Subtarget>();
  auto &FuncInfo = *MF.getInfo<llvm::X86MachineFunctionInfo>();
  llvm::MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto &TLI = *STI.getTargetLowering();
  auto ptrTy = TLI.getPointerTy(DAG.getDataLayout());

  if (!STI.is64Bit()) {
    return;
  }

  // Get the size of the stack, plus alignment to store the return
  // address for tail calls for the fast calling convention.
  unsigned stackSize = ci.GetFrameSize();
  switch (func_->GetCallingConv()) {
    case CallingConv::C: 
    case CallingConv::WIN64: {
      break;
    }
    case CallingConv::SETJMP:
    case CallingConv::CAML:
    case CallingConv::CAML_ALLOC:
    case CallingConv::CAML_GC:
    case CallingConv::XEN:
    case CallingConv::INTR:
    case CallingConv::MULTIBOOT: {
      Error(func_, "vararg call not supported");
    }
  }

  int index = MFI.CreateFixedObject(1, stackSize, false);
  FuncInfo.setVarArgsFrameIndex(index);

  // Copy all unused regs to be pushed on the stack into vregs.
  llvm::SmallVector<SDValue, 6> liveGPRs;
  llvm::SmallVector<SDValue, 8> liveXMMs;
  SDValue alReg;

  SDValue chain = DAG.getRoot();
  for (unsigned reg : ci.GetUnusedGPRs()) {
    unsigned vreg = MF.addLiveIn(reg, &X86::GR64RegClass);
    liveGPRs.push_back(DAG.getCopyFromReg(chain, SDL_, vreg, MVT::i64));
  }

  if (STI.hasSSE1()) {
    for (unsigned reg : ci.GetUnusedXMMs()) {
      if (!alReg) {
        unsigned vreg = MF.addLiveIn(X86::AL, &X86::GR8RegClass);
        alReg = DAG.getCopyFromReg(chain, SDL_, vreg, MVT::i8);
      }
      unsigned vreg = MF.addLiveIn(reg, &X86::VR128RegClass);
      liveXMMs.push_back(DAG.getCopyFromReg(chain, SDL_, vreg, MVT::v4f32));
    }
  }

  // Save the indices to be stored in __va_list_tag
  unsigned numGPRs = ci.GetUnusedGPRs().size() + ci.GetUsedGPRs().size();
  unsigned numXMMs = ci.GetUnusedXMMs().size() + ci.GetUsedXMMs().size();
  FuncInfo.setVarArgsGPOffset(ci.GetUsedGPRs().size() * 8);
  if (STI.hasSSE1()) {
    FuncInfo.setVarArgsFPOffset(numGPRs * 8 + ci.GetUsedXMMs().size() * 16);
  } else {
    FuncInfo.setVarArgsFPOffset(numGPRs * 8);
  }
  FuncInfo.setRegSaveFrameIndex(MFI.CreateStackObject(
      numGPRs * 8 + numXMMs * 16,
      llvm::Align(16),
      false
  ));

  llvm::SmallVector<SDValue, 8> storeOps;
  SDValue frameIdx = DAG.getFrameIndex(
      FuncInfo.getRegSaveFrameIndex(),
      ptrTy
  );

  // Store the unused GPR registers on the stack.
  unsigned gpOffset = FuncInfo.getVarArgsGPOffset();
  for (SDValue val : liveGPRs) {
    SDValue valIdx = DAG.getNode(
        ISD::ADD,
        SDL_,
        ptrTy,
        frameIdx,
        DAG.getIntPtrConstant(gpOffset, SDL_)
    );
    storeOps.push_back(DAG.getStore(
        val.getValue(1),
        SDL_,
        val,
        valIdx,
        llvm::MachinePointerInfo::getFixedStack(
            DAG.getMachineFunction(),
            FuncInfo.getRegSaveFrameIndex(),
            gpOffset
        )
    ));
    gpOffset += 8;
  }

  // Store the unused XMMs on the stack.
  if (!liveXMMs.empty()) {
    llvm::SmallVector<SDValue, 12> ops;
    ops.push_back(chain);
    ops.push_back(alReg);
    ops.push_back(DAG.getTargetConstant(
        FuncInfo.getRegSaveFrameIndex(),
        SDL_,
        MVT::i32
    ));
    ops.push_back(DAG.getTargetConstant(
        FuncInfo.getVarArgsFPOffset(),
        SDL_,
        MVT::i32
    ));
    ops.insert(ops.end(), liveXMMs.begin(), liveXMMs.end());
    storeOps.push_back(DAG.getNode(
        X86ISD::VASTART_SAVE_XMM_REGS,
        SDL_,
        MVT::Other,
        ops
    ));
  }

  if (!storeOps.empty()) {
    chain = DAG.getNode(ISD::TokenFactor, SDL_, MVT::Other, storeOps);
  }

  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
bool X86ISel::Finalize(llvm::MachineFunction &MF)
{
  llvm::X86::CreateGlobalBaseReg(MF);
  return true;
}

// -----------------------------------------------------------------------------
SDValue X86ISel::GetRegArch(Register reg)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  auto &TLI = *MF.getSubtarget().getTargetLowering();
  auto ptrVT = TLI.getPointerTy(DAG.getDataLayout());

  auto mov = [&, this] (const char *code, MVT ty)
  {
    auto reg = MRI.createVirtualRegister(TLI.getRegClassFor(ty));
    auto n = LowerInlineAsm(
        ISD::INLINEASM,
        DAG.getRoot(),
        code,
        0,
        { },
        { X86::DF, X86::FPSW, X86::EFLAGS },
        { reg }
    );

    auto copy = DAG.getCopyFromReg(n.getValue(0), SDL_, reg, ty, n.getValue(1));
    DAG.setRoot(copy.getValue(1));
    return copy.getValue(0);
  };

  switch (reg) {
    case Register::FS:      return mov("mov %fs:0, $0", ptrVT);
    case Register::X86_CR0: return mov("mov  %cr0, $0", ptrVT);
    case Register::X86_CR2: return mov("mov  %cr2, $0", ptrVT);
    case Register::X86_CR3: return mov("mov  %cr3, $0", ptrVT);
    case Register::X86_CR4: return mov("mov  %cr4, $0", ptrVT);
    default: {
      llvm_unreachable("invalid register");
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSetSP(SDValue value)
{
  auto &DAG = GetDAG();
  DAG.setRoot(DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      GetStackRegister(),
      value
  ));
}

// -----------------------------------------------------------------------------
llvm::SDValue X86ISel::LowerCallee(ConstRef<Inst> inst)
{
  auto &DAG = GetDAG();
  auto ptrVT = DAG.getTargetLoweringInfo().getPointerTy(DAG.getDataLayout());

  if (ConstRef<MovInst> movInst = ::cast_or_null<MovInst>(inst)) {
    ConstRef<Value> movArg = GetMoveArg(movInst.Get());
    switch (movArg->GetKind()) {
      case Value::Kind::INST: {
        return GetValue(::cast<Inst>(movArg));
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
                  ptrVT,
                  0,
                  llvm::X86II::MO_NO_FLAG
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
        return GetValue(movInst);
      }
    }
    llvm_unreachable("invalid value kind");
  } else {
    return GetValue(inst);
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerCallSite(SDValue chain, const CallSite *call)
{
  const Block *block = call->getParent();
  const Func *func = block->getParent();

  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  const auto &STI = MF.getSubtarget();
  const auto &TRI = *STI.getRegisterInfo();
  const auto &TLI = *STI.getTargetLowering();
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();
  auto ptrTy = TLI.getPointerTy(DAG.getDataLayout());

  // Analyse the arguments, finding registers for them.
  bool isVarArg = call->IsVarArg();
  bool isTailCall = call->Is(Inst::Kind::TAIL_CALL);
  bool isInvoke = call->Is(Inst::Kind::INVOKE);
  bool wasTailCall = isTailCall;
  std::unique_ptr<X86Call> ci(GetCallLowering(GetDAG(), call));

  // Find the number of bytes allocated to hold arguments.
  unsigned stackSize = ci->GetFrameSize();

  // Compute the stack difference for tail calls.
  int fpDiff = 0;
  if (isTailCall) {
    std::unique_ptr<X86Call> ci(GetCallLowering(GetDAG(), func));
    int bytesToPop;
    switch (func->GetCallingConv()) {
      default: {
        llvm_unreachable("invalid C calling convention");
      }
      case CallingConv::C: {
        if (func->IsVarArg()) {
          bytesToPop = ci->GetFrameSize();
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
  const uint32_t *mask = TRI.getCallPreservedMask(MF, cc);

  // Instruction bundle starting the call.
  if (needsAdjust) {
    chain = DAG.getCALLSEQ_START(chain, stackSize, 0, SDL_);
  }

  // Identify registers and stack locations holding the arguments.
  llvm::SmallVector<std::pair<unsigned, SDValue>, 8> regArgs;
  chain = LowerCallArguments(chain, call, *ci, regArgs);

  if (isVarArg) {
    // If XMM regs are used, their count needs to be passed in AL.
    unsigned count = 0;
    for (auto arg : call->args()) {
      if (IsFloatType(arg.GetType())) {
        count = std::min(8u, count + 1);
      }
    }

    regArgs.push_back({ X86::AL, DAG.getConstant(count, SDL_, MVT::i8) });
  }

  if (isTailCall) {
    // Shuffle arguments on the stack.
    for (auto it = ci->arg_begin(); it != ci->arg_end(); ++it) {
      for (auto &part : it->Parts) {
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
    regArgs.emplace_back(X86::RAX, GetValue(call->GetCallee()));
    callee = DAG.getTargetGlobalAddress(
        trampoline_,
        SDL_,
        MVT::i64,
        0,
        llvm::X86II::MO_NO_FLAG
    );
  } else {
    callee = LowerCallee(call->GetCallee());
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
    DAG.setRoot(DAG.getNode(X86ISD::TC_RETURN, SDL_, nodeTypes, ops));
  } else {
    chain = DAG.getNode(X86ISD::CALL, SDL_, nodeTypes, ops);
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
          returns.push_back(ci->Return(i));
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
      ops.push_back(DAG.getTargetConstant(0, SDL_, MVT::i32));
      for (auto &reg : regs) {
        ops.push_back(reg);
      }

      chain = DAG.getNode(
          X86ISD::RET_FLAG,
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
void X86ISel::LowerSyscall(const SyscallInst *inst)
{
  // 32-bit registers.
  static std::vector<llvm::Register> kRetRegs32 = { X86::EAX };
  static std::vector<llvm::Register> kArgRegs32 = {
    X86::EBX, X86::ECX, X86::EDX, X86::ESI, X86::EDI, X86::EBP
  };
  // 64-bit registers.
  static std::vector<llvm::Register> kRetRegs64 = { X86::RAX };
  static std::vector<llvm::Register> kArgRegs64 = {
      X86::RDI, X86::RSI, X86::RDX, X86::R10, X86::R8, X86::R9
  };

  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto PtrVT = GetPointerType();
  bool is32Bit = PtrVT == MVT::i32;

  llvm::SmallVector<SDValue, 7> ops;
  SDValue chain = DAG.getRoot();
  SDValue glue;

  // Lower arguments.
  unsigned args = 0;
  {
    for (ConstRef<Inst> arg : inst->args()) {
      llvm::Register reg;
      if (is32Bit) {
        if (args >= kArgRegs32.size()) {
          Error(inst, "too many arguments to syscall");
        }
        reg = kArgRegs32[args++];
      } else {
        if (args >= kArgRegs64.size()) {
          Error(inst, "too many arguments to syscall");
        }
        reg = kArgRegs64[args++];
      }

      SDValue value = DAG.getAnyExtOrTrunc(GetValue(arg), SDL_, PtrVT);
      ops.push_back(DAG.getRegister(reg, PtrVT));
      SDValue copy = DAG.getCopyToReg(chain, SDL_, reg, value, glue);
      chain = copy.getValue(0);
      glue = copy.getValue(1);
    }
  }

  /// Lower to the syscall.
  {
    ops.push_back(DAG.getRegister(X86::RAX, PtrVT));

    SDValue copy = DAG.getCopyToReg(
        chain,
        SDL_,
        is32Bit ? X86::EAX : X86::RAX,
        DAG.getZExtOrTrunc(
            GetValue(inst->GetSyscall()),
            SDL_,
            PtrVT
        ),
        glue
    );
    chain = copy.getValue(0);
    glue = copy.getValue(1);

    ops.push_back(chain);
    ops.push_back(glue);

    SDValue syscall = SDValue(DAG.getMachineNode(
        X86::SYSCALL,
        SDL_,
        DAG.getVTList(MVT::Other, MVT::Glue),
        ops
    ), 0);

    chain = syscall.getValue(0);
    glue = syscall.getValue(1);
  }

  /// Copy the return value into a vreg and export it.
  for (unsigned i = 0, n = inst->type_size(); i < n; ++i) {
    auto ty = inst->type(i);
    llvm::Register reg;
    if (is32Bit) {
      if (ty != Type::I32 || i >= kRetRegs32.size()) {
        Error(inst, "invalid syscall type");
      }
      reg = kRetRegs32[i];
    } else {
      if (ty != Type::I64 || i >= kRetRegs64.size()) {
        Error(inst, "invalid syscall type");
      }
      reg = kRetRegs64[i];
    }

    SDValue copy = DAG.getCopyFromReg(chain, SDL_, reg, PtrVT, glue);
    chain = copy.getValue(1);
    glue = copy.getValue(2);
    Export(inst->GetSubValue(i), copy.getValue(0));
  }

  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerClone(const CloneInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &STI = MF.getSubtarget();
  const auto &TLI = *STI.getTargetLowering();

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

  CopyReg(inst->GetFlags(), X86::RDI);
  CopyReg(inst->GetStack(), X86::RSI);
  CopyReg(inst->GetPTID(), X86::RDX);
  CopyReg(inst->GetCTID(), X86::R10);
  CopyReg(inst->GetTLS(), X86::R8);

  chain = LowerInlineAsm(
      ISD::INLINEASM,
      chain,
      "and $$-16, %rsi;"
      "sub $$16, %rsi;"
      "mov $2, (%rsi);"
      "mov $1, 8(%rsi);"
      "mov $$56, %eax;"
      "syscall;"
      "test %eax, %eax;"
      "jnz 1f;"
      "xor %ebp, %ebp;"
      "pop %rdi;"
      "pop %r9;"
      "call *%r9;"
      "mov %eax, %edi;"
      "mov $$60, %eax;"
      "syscall;"
      "hlt;"
      "1:",
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      { callee, arg, X86::RDI, X86::RSI, X86::RDX, X86::R10, X86::R8 },
      { X86::DF, X86::FPSW, X86::EFLAGS },
      { X86::RAX },
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
        X86::RAX,
        MVT::i64,
        chain.getValue(1)
    ).getValue(1);

    Export(inst, chain.getValue(0));
  }

  // Update the root.
  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerRaise(const RaiseInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &STI = MF.getSubtarget();
  const auto &TLI = *STI.getTargetLowering();
  auto PtrVT = GetPointerType();

  // Copy in the new stack pointer and code pointer.
  auto stk = MRI.createVirtualRegister(TLI.getRegClassFor(PtrVT));
  SDValue stkNode = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      stk,
      GetValue(inst->GetStack()),
      SDValue()
  );
  auto pc = MRI.createVirtualRegister(TLI.getRegClassFor(PtrVT));
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
    std::unique_ptr<X86Call> ci(GetCallLowering(GetDAG(), inst));
    std::tie(chain, glue) = LowerRaises(
        chain,
        *ci,
        inst,
        regs,
        glue
    );
  } else {
    if (!inst->arg_empty()) {
      Error(inst, "missing calling convention");
    }
  }

  if (MF.getSubtarget<llvm::X86Subtarget>().is64Bit()) {
    DAG.setRoot(LowerInlineAsm(
        ISD::INLINEASM_BR,
        chain,
        "movq $0, %rsp\n"
        "jmp *$1",
        0,
        regs,
        { },
        { },
        glue
    ));
  } else {
    DAG.setRoot(LowerInlineAsm(
        ISD::INLINEASM_BR,
        chain,
        "movl $0, %esp\n"
        "jmp *$1",
        0,
        regs,
        { },
        { },
        glue
    ));
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSpawn(const SpawnInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &STI = MF.getSubtarget();
  const auto &TLI = *STI.getTargetLowering();

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
  DAG.setRoot(LowerInlineAsm(
      ISD::INLINEASM_BR,
      DAG.getRoot(),
      "movq $0, %rsp\n"
      "jmp *$1",
      0,
      { stk, pc },
      { },
      { },
      pcNode.getValue(1)
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_CmpXchgInst *inst)
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

  SDValue Swap = DAG.getAtomicCmpSwap(
      ISD::ATOMIC_CMP_SWAP_WITH_SUCCESS,
      SDL_,
      retTy,
      DAG.getVTList(retTy, MVT::i1, MVT::Other),
      DAG.getRoot(),
      GetValue(inst->GetAddr()),
      GetValue(inst->GetRef()),
      GetValue(inst->GetValue()),
      mmo
  );
  DAG.setRoot(Swap.getValue(2));
  Export(inst, Swap.getValue(0));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_FnClExInst *)
{
  auto &DAG = GetDAG();
  SDValue Ops[] = { DAG.getRoot() };
  DAG.setRoot(
      DAG.getNode(
          X86ISD::FNCLEX,
          SDL_,
          DAG.getVTList(MVT::Other),
          Ops
      )
  );
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_RdTscInst *inst)
{
  auto &DAG = GetDAG();
  SDValue node = DAG.getNode(
      ISD::READCYCLECOUNTER,
      SDL_,
      DAG.getVTList(MVT::i64, MVT::Other),
      DAG.getRoot()
  );
  DAG.setRoot(node.getValue(1));
  Export(inst, node.getValue(0));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const BarrierInst *inst, unsigned op)
{
  auto &DAG = GetDAG();
  DAG.setRoot(DAG.getNode(op, SDL_, DAG.getVTList(MVT::Other), DAG.getRoot()));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_CpuIdInst *inst)
{
  auto &DAG = GetDAG();

  SDValue raxNode = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      X86::EAX,
      DAG.getAnyExtOrTrunc(GetValue(inst->GetLeaf()), SDL_, MVT::i32),
      SDValue()
  );
  SDValue chain = raxNode.getValue(0);
  SDValue glue = raxNode.getValue(1);

  SDValue subleaf;
  if (auto node = inst->GetSubleaf()) {
    subleaf = DAG.getAnyExtOrTrunc(GetValue(node), SDL_, MVT::i32);
  } else {
    subleaf = DAG.getUNDEF(MVT::i32);
  }
  SDValue rcxNode = DAG.getCopyToReg(chain, SDL_, X86::ECX, subleaf, glue);
  chain = rcxNode.getValue(0);
  glue = rcxNode.getValue(1);

  std::vector<llvm::Register> outRegs{ X86::EAX, X86::EBX, X86::ECX, X86::EDX };
  SDValue cpuid = SDValue(DAG.getMachineNode(
      X86::CPUID,
      SDL_,
      DAG.getVTList(MVT::Other, MVT::Glue),
      { chain, glue }
  ), 0);
  chain = cpuid.getValue(0);
  glue = cpuid.getValue(1);

  for (unsigned i = 0, n = inst->GetNumRets(); i < n; ++i) {
    SDValue copy = DAG.getCopyFromReg(
        chain,
        SDL_,
        outRegs[i],
        MVT::i32,
        glue
    );
    chain = copy.getValue(1);
    glue = copy.getValue(2);

    Export(inst->GetSubValue(i), DAG.getAnyExtOrTrunc(
        copy.getValue(0),
        SDL_,
        GetVT(inst->GetType(i))
    ));
  }
  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_XchgInst *inst)
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
      GetValue(inst->GetValue()),
      mmo
  );

  dag.setRoot(xchg.getValue(1));
  Export(inst, xchg.getValue(0));
}

// -----------------------------------------------------------------------------
static std::pair<unsigned, llvm::Register>
GetInOps(Type ty)
{
  switch (ty) {
    default: llvm::report_fatal_error("invalid x86_in instruction");
    case Type::I8:  return { X86::IN8rr, X86::AL };
    case Type::I16: return { X86::IN16rr, X86::AX };
    case Type::I32: return { X86::IN32rr, X86::EAX };
  }
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_InInst *inst)
{
  auto &DAG = GetDAG();
  auto [op, reg] = GetInOps(inst->GetType());

  SDValue port = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      X86::DX,
      DAG.getAnyExtOrTrunc(GetValue(inst->GetPort()), SDL_, MVT::i16),
      SDValue()
  );
  SDValue chain = port.getValue(0);
  SDValue glue = port.getValue(1);

  chain = SDValue(DAG.getMachineNode(
      op,
      SDL_,
      DAG.getVTList(MVT::Other, MVT::Glue),
      { chain, glue }
  ), 0);

  chain = DAG.getCopyFromReg(
      chain,
      SDL_,
      reg,
      GetVT(inst->GetType()),
      chain.getValue(1)
  ).getValue(1);

  Export(inst, chain.getValue(0));
  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
static std::pair<unsigned, llvm::Register>
GetOutOps(Type ty)
{
  switch (ty) {
    default: llvm::report_fatal_error("invalid x86_out instruction");
    case Type::I8:  return { X86::OUT8rr, X86::AL };
    case Type::I16: return { X86::OUT16rr, X86::AX };
    case Type::I32: return { X86::OUT32rr, X86::EAX };
  }
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_OutInst *inst)
{
  auto &DAG = GetDAG();

  auto valTy = inst->GetValue().GetType();
  auto [op, reg] = GetOutOps(valTy);

  SDValue port = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      X86::DX,
      DAG.getAnyExtOrTrunc(GetValue(inst->GetPort()), SDL_, MVT::i16),
      SDValue()
  );
  SDValue chain = port.getValue(0);
  SDValue glue = port.getValue(1);

  SDValue value = DAG.getCopyToReg(
      chain,
      SDL_,
      reg,
      DAG.getAnyExtOrTrunc(GetValue(inst->GetValue()), SDL_, GetVT(valTy)),
      glue
  );
  chain = value.getValue(0);
  glue = value.getValue(1);

  DAG.setRoot(SDValue(DAG.getMachineNode(
      op,
      SDL_,
      MVT::Other,
      { chain, glue }
  ), 0));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_WrMsrInst *inst)
{
  auto &DAG = GetDAG();

  SDValue ecx = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      X86::ECX,
      DAG.getAnyExtOrTrunc(GetValue(inst->GetReg()), SDL_, MVT::i32),
      SDValue()
  );
  SDValue chain = ecx.getValue(0);
  SDValue glue = ecx.getValue(1);

  SDValue eax = DAG.getCopyToReg(
      chain,
      SDL_,
      X86::EAX,
      DAG.getAnyExtOrTrunc(GetValue(inst->GetLo()), SDL_, MVT::i32),
      glue
  );
  chain = eax.getValue(0);
  glue = eax.getValue(1);

  SDValue edx = DAG.getCopyToReg(
      chain,
      SDL_,
      X86::EDX,
      DAG.getAnyExtOrTrunc(GetValue(inst->GetHi()), SDL_, MVT::i32),
      glue
  );
  chain = edx.getValue(0);
  glue = edx.getValue(1);

  DAG.setRoot(SDValue(DAG.getMachineNode(
      X86::WRMSR,
      SDL_,
      MVT::Other,
      { chain, glue }
  ), 0));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_RdMsrInst *inst)
{
  auto &DAG = GetDAG();

  SDValue ecx = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      X86::ECX,
      DAG.getAnyExtOrTrunc(GetValue(inst->GetReg()), SDL_, MVT::i32),
      SDValue()
  );

  SDValue chain = SDValue(DAG.getMachineNode(
      X86::RDMSR,
      SDL_,
      { MVT::Other, MVT::Glue },
      { ecx.getValue(0), ecx.getValue(1) }
  ), 0);
  SDValue glue = chain.getValue(1);

  static llvm::Register kRegs[] = { X86::EAX, X86::EDX };
  for (unsigned i = 0; i < 2; ++i) {
    SDValue reg = DAG.getCopyFromReg(
        chain,
        SDL_,
        kRegs[i],
        MVT::i32,
        glue
    );
    MVT ty = GetVT(inst->GetType(i));
    Export(inst->GetSubValue(i), DAG.getAnyExtOrTrunc(reg, SDL_, ty));
    chain = reg.getValue(1);
    glue = reg.getValue(2);
  }

  DAG.setRoot(chain);
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_PauseInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(SDValue(DAG.getMachineNode(
      X86::PAUSE,
      SDL_,
      MVT::Other,
      DAG.getRoot()
  ), 0));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_YieldInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(LowerInlineAsm(
      ISD::INLINEASM,
      DAG.getRoot(),
      "rep; nop",
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      { },
      { X86::EFLAGS },
      { }
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_StiInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(LowerInlineAsm(
      ISD::INLINEASM,
      DAG.getRoot(),
      "sti",
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      { },
      { X86::EFLAGS },
      { }
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_CliInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(LowerInlineAsm(
      ISD::INLINEASM,
      DAG.getRoot(),
      "cli",
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      { },
      { X86::EFLAGS },
      { }
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_SpinInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(LowerInlineAsm(
      ISD::INLINEASM,
      DAG.getRoot(),
      "sti; nop; cli",
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      { },
      { X86::EFLAGS },
      { }
  ));
}


// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_HltInst *inst)
{
  auto &DAG = GetDAG();
  DAG.setRoot(SDValue(DAG.getMachineNode(
      X86::HLT,
      SDL_,
      MVT::Other,
      DAG.getRoot()
  ), 0));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_LgdtInst *inst)
{
  LowerSetReg("lgdt ($0)", GetPointerType(), GetValue(inst->GetValue()));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_LidtInst *inst)
{
  LowerSetReg("lidt ($0)", GetPointerType(), GetValue(inst->GetValue()));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_LtrInst *inst)
{
  LowerSetReg("ltr $0", MVT::i16, GetValue(inst->GetValue()));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSetReg(const char *code, MVT type, SDValue value)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &STI = MF.getSubtarget();
  const auto &TLI = *STI.getTargetLowering();

  // Copy in the new stack pointer and code pointer.
  auto reg = MRI.createVirtualRegister(TLI.getRegClassFor(type));
  SDValue addrNode = DAG.getCopyToReg(
      DAG.getRoot(),
      SDL_,
      reg,
      DAG.getAnyExtOrTrunc(value, SDL_, type),
      SDValue()
  );

  DAG.setRoot(LowerInlineAsm(
      ISD::INLINEASM,
      addrNode.getValue(0),
      code,
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      { reg },
      { },
      { },
      addrNode.getValue(1)
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_XSaveOptInst *inst)
{
  return LowerContextMask(
      false,
      llvm::Intrinsic::x86_xsaveopt64,
      GetValue(inst->GetAddr()),
      GetValue(inst->GetRFBM())
  );
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const X86_XRestoreInst *inst)
{
  return LowerContextMask(
      true,
      llvm::Intrinsic::x86_xrstor64,
      GetValue(inst->GetAddr()),
      GetValue(inst->GetRFBM())
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerContextMask(bool store, unsigned op, SDValue addr, SDValue mask)
{
  auto &DAG = GetDAG();

  auto lo = DAG.getNode(ISD::TRUNCATE, SDL_, MVT::i32, mask);
  auto hi = DAG.getNode(
      ISD::TRUNCATE,
      SDL_,
      MVT::i32,
      DAG.getNode(
          ISD::SRL,
          SDL_,
          MVT::i64,
          mask,
          DAG.getConstant(32, SDL_, MVT::i8)
      )
  );

  DAG.setRoot(DAG.getMemIntrinsicNode(
      ISD::INTRINSIC_VOID,
      SDL_,
      DAG.getVTList(MVT::Other),
      {
        DAG.getRoot(),
        DAG.getIntPtrConstant(op, SDL_, true),
        addr,
        hi,
        lo
      },
      MVT::i64,
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::Align(1),
      store ? llvm::MachineMemOperand::MOStore : llvm::MachineMemOperand::MOLoad
  ));
}

// -----------------------------------------------------------------------------
llvm::Register X86ISel::GetStackRegister() const
{
  return GetPointerType() == MVT::i64 ? X86::RSP : X86::ESP;
}
