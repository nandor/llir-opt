// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <bitset>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/InlineAsm.h>
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
    case Inst::Kind::X86_XCHG:    return LowerXchg(static_cast<const X86_XchgInst *>(i));
    case Inst::Kind::X86_CMPXCHG: return LowerCmpXchg(static_cast<const X86_CmpXchgInst *>(i));
    case Inst::Kind::X86_FNCLEX:  return LowerFnClEx(static_cast<const X86_FnClExInst *>(i));
    case Inst::Kind::X86_FNSTCW:  return LowerFPUControl(X86ISD::FNSTCW16m, 2, true, i);
    case Inst::Kind::X86_FNSTSW:  return LowerFPUControl(X86ISD::FNSTSW16m, 2, true, i);
    case Inst::Kind::X86_FNSTENV: return LowerFPUControl(X86ISD::FNSTENVm, 28, true, i);
    case Inst::Kind::X86_FLDCW:   return LowerFPUControl(X86ISD::FLDCW16m, 2, false, i);
    case Inst::Kind::X86_FLDENV:  return LowerFPUControl(X86ISD::FLDENVm, 28, false, i);
    case Inst::Kind::X86_LDMXCSR: return LowerFPUControl(X86ISD::LDMXCSR32m, 4, false, i);
    case Inst::Kind::X86_STMXCSR: return LowerFPUControl(X86ISD::STMXCSR32m, 4, true, i);
    case Inst::Kind::X86_RDTSC:   return LowerRdtsc(static_cast<const X86_RdtscInst *>(i));
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerFnClEx(const X86_FnClExInst *)
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
void X86ISel::LowerRdtsc(const X86_RdtscInst *inst)
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
void X86ISel::LowerReturn(const ReturnInst *retInst)
{
  auto &DAG = GetDAG();
  llvm::SmallVector<SDValue, 6> ops;
  ops.push_back(SDValue());
  ops.push_back(DAG.getTargetConstant(0, SDL_, MVT::i32));

  X86Call ci(retInst);
  auto [chain, flag] = LowerRets(GetExportRoot(), ci, retInst, ops);

  ops[0] = chain;
  if (flag.getNode()) {
    ops.push_back(flag);
  }

  DAG.setRoot(DAG.getNode(X86ISD::RET_FLAG, SDL_, MVT::Other, ops));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerCmpXchg(const X86_CmpXchgInst *inst)
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
      GetValue(inst->GetVal()),
      mmo
  );
  DAG.setRoot(Swap.getValue(2));
  Export(inst, Swap.getValue(0));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSet(const SetInst *inst)
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
    // Architecture-specific registers.
    case ConstantReg::Kind::AARCH64_FPSR:
    case ConstantReg::Kind::AARCH64_FPCR:
    case ConstantReg::Kind::RISCV_FFLAGS:
    case ConstantReg::Kind::RISCV_FRM:
    case ConstantReg::Kind::RISCV_FCSR:
    case ConstantReg::Kind::PPC_FPSCR:  {
      llvm_unreachable("invalid register");
    }
  }
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
  SDValue Ops[] = { DAG.getRoot(), addr };
  DAG.setRoot(
      DAG.getMemIntrinsicNode(
          opcode,
          SDL_,
          DAG.getVTList(MVT::Other),
          Ops,
          MVT::i16,
          mmo
      )
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerArguments(bool hasVAStart)
{
  X86Call lowering(func_);
  if (hasVAStart) {
    LowerVASetup(lowering);
  }
  LowerArgs(lowering);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerLandingPad(const LandingPadInst *inst)
{
  LowerPad(X86Call(inst), inst);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerVASetup(const X86Call &ci)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &FuncInfo = *MF.getInfo<llvm::X86MachineFunctionInfo>();
  llvm::MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto &TLI = *MF.getSubtarget().getTargetLowering();
  auto ptrTy = TLI.getPointerTy(DAG.getDataLayout());

  // Get the size of the stack, plus alignment to store the return
  // address for tail calls for the fast calling convention.
  unsigned stackSize = ci.GetFrameSize();
  switch (func_->GetCallingConv()) {
    case CallingConv::C: {
      break;
    }
    case CallingConv::SETJMP:
    case CallingConv::CAML:
    case CallingConv::CAML_ALLOC:
    case CallingConv::CAML_GC: {
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

  for (unsigned reg : ci.GetUnusedXMMs()) {
    if (!alReg) {
      unsigned vreg = MF.addLiveIn(X86::AL, &X86::GR8RegClass);
      alReg = DAG.getCopyFromReg(chain, SDL_, vreg, MVT::i8);
    }
    unsigned vreg = MF.addLiveIn(reg, &X86::VR128RegClass);
    liveXMMs.push_back(DAG.getCopyFromReg(chain, SDL_, vreg, MVT::v4f32));
  }

  // Save the indices to be stored in __va_list_tag
  unsigned numGPRs = ci.GetUnusedGPRs().size() + ci.GetUsedGPRs().size();
  unsigned numXMMs = ci.GetUnusedXMMs().size() + ci.GetUsedXMMs().size();
  FuncInfo.setVarArgsGPOffset(ci.GetUsedGPRs().size() * 8);
  FuncInfo.setVarArgsFPOffset(numGPRs * 8 + ci.GetUsedXMMs().size() * 16);
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
SDValue X86ISel::LoadRegArch(ConstantReg::Kind reg)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  const auto &TLI = *MF.getSubtarget().getTargetLowering();

  switch (reg) {
    case ConstantReg::Kind::FS: {
      auto reg = MRI.createVirtualRegister(TLI.getRegClassFor(MVT::i64));
      auto node = LowerInlineAsm(
          ISD::INLINEASM,
          DAG.getRoot(),
          "mov %fs:0,$0",
          0,
          { },
          { X86::DF, X86::FPSW, X86::EFLAGS },
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
    }
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
      X86::RSP,
      value
  ));
}

// -----------------------------------------------------------------------------
llvm::SDValue X86ISel::LowerCallee(ConstRef<Inst> inst)
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
        llvm_unreachable("invalid call argument");
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
  bool isTailCall = call->Is(Inst::Kind::TCALL);
  bool isInvoke = call->Is(Inst::Kind::INVOKE);
  bool wasTailCall = isTailCall;
  X86Call locs(call);

  // Find the number of bytes allocated to hold arguments.
  unsigned stackSize = locs.GetFrameSize();

  // Compute the stack difference for tail calls.
  int fpDiff = 0;
  if (isTailCall) {
    X86Call callee(func);
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
  const uint32_t *mask = TRI.getCallPreservedMask(MF, cc);

  // Instruction bundle starting the call.
  if (needsAdjust) {
    chain = DAG.getCALLSEQ_START(chain, stackSize, 0, SDL_);
  }

  // Identify registers and stack locations holding the arguments.
  llvm::SmallVector<std::pair<unsigned, SDValue>, 8> regArgs;
  chain = LowerCallArguments(chain, call, locs, regArgs);

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
    for (auto it = locs.arg_begin(); it != locs.arg_end(); ++it) {
      for (auto &part : it->Parts) {
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
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();

  static unsigned kRegs[] = {
      X86::RDI, X86::RSI, X86::RDX,
      X86::R10, X86::R8, X86::R9
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
    ops.push_back(DAG.getRegister(X86::RAX, MVT::i64));

    chain = DAG.getCopyToReg(
        chain,
        SDL_,
        X86::RAX,
        GetValue(inst->GetSyscall())
    );

    ops.push_back(chain);

    chain = SDValue(DAG.getMachineNode(
        X86::SYSCALL,
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
          X86::RAX,
          MVT::i64,
          chain.getValue(1)
      ).getValue(1);

      Export(inst, chain.getValue(0));
    }
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
        X86Call(inst),
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
      "movq $0, %rsp\n"
      "jmp *$1",
      0,
      regs,
      { },
      { },
      glue
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerXchg(const X86_XchgInst *inst)
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
