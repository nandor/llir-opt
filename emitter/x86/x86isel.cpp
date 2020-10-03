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
using BranchProbability = llvm::BranchProbability;



// -----------------------------------------------------------------------------
BranchProbability kLikely = BranchProbability::getBranchProbability(99, 100);
BranchProbability kUnlikely = BranchProbability::getBranchProbability(1, 100);

// -----------------------------------------------------------------------------
char X86ISel::ID;

// -----------------------------------------------------------------------------
X86ISel::X86ISel(
    llvm::X86TargetMachine *TM,
    llvm::X86Subtarget *STI,
    const llvm::X86InstrInfo *TII,
    const llvm::X86RegisterInfo *TRI,
    const llvm::TargetLowering *TLI,
    llvm::TargetLibraryInfo *LibInfo,
    const Prog *prog,
    llvm::CodeGenOpt::Level OL,
    bool shared)
  : DAGMatcher(*TM, new llvm::SelectionDAG(*TM, OL), OL, TLI, TII)
  , X86DAGMatcher(*TM, OL, STI)
  , ISel(ID, prog, LibInfo)
  , TM_(TM)
  , TRI_(TRI)
  , trampoline_(nullptr)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
void X86ISel::LowerReturn(const ReturnInst *retInst)
{
  llvm::SmallVector<SDValue, 6> returns;
  returns.push_back(SDValue());
  returns.push_back(CurDAG->getTargetConstant(0, SDL_, MVT::i32));

  SDValue flag;
  SDValue chain = GetExportRoot();
  if (auto *retVal = retInst->GetValue()) {
    Type retType = retVal->GetType(0);
    unsigned retReg;
    switch (retType) {
      case Type::I8:  retReg = X86::AL;   break;
      case Type::I16: retReg = X86::AX;   break;
      case Type::I64: retReg = X86::RAX;  break;
      case Type::I32: retReg = X86::EAX;  break;
      case Type::F32: retReg = X86::XMM0; break;
      case Type::F64: retReg = X86::XMM0; break;
      default: Error(retInst, "Invalid return type");
    }

    SDValue arg = GetValue(retVal);
    chain = CurDAG->getCopyToReg(chain, SDL_, retReg, arg, flag);
    returns.push_back(CurDAG->getRegister(retReg, GetType(retType)));
    flag = chain.getValue(1);
  }

  returns[0] = chain;
  if (flag.getNode()) {
    returns.push_back(flag);
  }

  CurDAG->setRoot(CurDAG->getNode(X86ISD::RET_FLAG, SDL_, MVT::Other, returns));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerCall(const CallInst *inst)
{
  LowerCallSite(CurDAG->getRoot(), inst);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerTailCall(const TailCallInst *inst)
{
  LowerCallSite(CurDAG->getRoot(), inst);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerInvoke(const InvokeInst *inst)
{
  auto &MMI = MF->getMMI();
  auto *bCont = inst->GetCont();
  auto *bThrow = inst->GetThrow();
  auto *mbbCont = blocks_[bCont];
  auto *mbbThrow = blocks_[bThrow];

  // Mark the landing pad as such.
  mbbThrow->setIsEHPad();

  // Lower the invoke call: export here since the call might not return.
  LowerCallSite(GetExportRoot(), inst);

  // Add a jump to the continuation block: export the invoke result.
  CurDAG->setRoot(CurDAG->getNode(
      ISD::BR,
      SDL_,
      MVT::Other,
      GetExportRoot(),
      CurDAG->getBasicBlock(mbbCont)
  ));

  // Mark successors.
  auto *sourceMBB = blocks_[inst->getParent()];
  sourceMBB->addSuccessor(mbbCont, BranchProbability::getOne());
  sourceMBB->addSuccessor(mbbThrow, BranchProbability::getZero());
  sourceMBB->normalizeSuccProbs();
}

// -----------------------------------------------------------------------------
void X86ISel::LowerTailInvoke(const TailInvokeInst *inst)
{
  auto &MMI = MF->getMMI();
  auto *bThrow = inst->GetThrow();
  auto *mbbThrow = blocks_[bThrow];

  // Mark the landing pad as such.
  mbbThrow->setIsEHPad();

  // Lower the invoke call.
  LowerCallSite(GetExportRoot(), inst);

  // Mark successors.
  auto *sourceMBB = blocks_[inst->getParent()];
  sourceMBB->addSuccessor(mbbThrow);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerCmpXchg(const CmpXchgInst *inst)
{
  unsigned reg;
  unsigned size;
  MVT type;
  switch (inst->GetType()) {
    case Type::I8:  reg = X86::AL;  size = 1; type = MVT::i8;  break;
    case Type::I16: reg = X86::AX;  size = 2; type = MVT::i16; break;
    case Type::I32: reg = X86::EAX; size = 4; type = MVT::i32; break;
    case Type::I64: reg = X86::RAX; size = 8; type = MVT::i64; break;
    case Type::I128: {
      Error(inst, "invalid type for atomic");
    }
    case Type::F32: case Type::F64: case Type::F80: {
      Error(inst, "invalid type for atomic");
    }
  }

  auto *mmo = MF->getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile |
      llvm::MachineMemOperand::MOLoad |
      llvm::MachineMemOperand::MOStore,
      GetSize(inst->GetType()),
      llvm::Align(size),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDValue writeReg = CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      reg,
      GetValue(inst->GetRef()),
      SDValue()
  );
  SDValue ops[] = {
      writeReg.getValue(0),
      GetValue(inst->GetAddr()),
      GetValue(inst->GetVal()),
      CurDAG->getTargetConstant(size, SDL_, MVT::i8),
      writeReg.getValue(1)
   };
  SDValue cmpXchg = CurDAG->getMemIntrinsicNode(
      X86ISD::LCMPXCHG_DAG,
      SDL_,
      CurDAG->getVTList(MVT::Other, MVT::Glue),
      ops,
      type,
      mmo
  );
  SDValue readReg = CurDAG->getCopyFromReg(
      cmpXchg.getValue(0),
      SDL_,
      reg,
      type,
      cmpXchg.getValue(1)
  );
  CurDAG->setRoot(readReg.getValue(1));
  Export(inst, readReg.getValue(0));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSet(const SetInst *inst)
{
  auto value = GetValue(inst->GetValue());
  switch (inst->GetReg()->GetValue()) {
    // Stack pointer.
    case ConstantReg::Kind::RSP: return LowerSetRSP(value);
    // X86 architectural register.
    case ConstantReg::Kind::RAX:
    case ConstantReg::Kind::RBX:
    case ConstantReg::Kind::RCX:
    case ConstantReg::Kind::RDX:
    case ConstantReg::Kind::RSI:
    case ConstantReg::Kind::RDI:
    case ConstantReg::Kind::RBP:
    case ConstantReg::Kind::R8:
    case ConstantReg::Kind::R9:
    case ConstantReg::Kind::R10:
    case ConstantReg::Kind::R11:
    case ConstantReg::Kind::R12:
    case ConstantReg::Kind::R13:
    case ConstantReg::Kind::R14:
    case ConstantReg::Kind::R15: {
      Error(inst, "Cannot rewrite generic register");
    }
    // TLS base.
    case ConstantReg::Kind::FS: {
      Error(inst, "Cannot rewrite tls base");
    }
    // Program counter.
    case ConstantReg::Kind::PC: {
      Error(inst, "Cannot rewrite program counter");
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
void X86ISel::LowerVAStart(const VAStartInst *inst)
{
  if (!inst->getParent()->getParent()->IsVarArg()) {
    Error(inst, "vastart in a non-vararg function");
  }

  CurDAG->setRoot(CurDAG->getNode(
      ISD::VASTART,
      SDL_,
      MVT::Other,
      CurDAG->getRoot(),
      GetValue(inst->GetVAList()),
      CurDAG->getSrcValue(nullptr)
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerRDTSC(const RdtscInst *inst)
{
  switch (inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      llvm_unreachable("not implemented");
    }
    case Type::I64: {
      SDVTList Tys = CurDAG->getVTList(MVT::Other, MVT::Glue);
      SDValue Read = SDValue(CurDAG->getMachineNode(
          X86::RDTSC,
          SDL_,
          Tys,
          CurDAG->getRoot()
      ), 0);

      SDValue LO = CurDAG->getCopyFromReg(
          Read,
          SDL_,
          X86::RAX,
          MVT::i64,
          Read.getValue(1)
      );
      SDValue HI = CurDAG->getCopyFromReg(
          LO.getValue(1),
          SDL_,
          X86::RDX,
          MVT::i64,
          LO.getValue(2)
      );

      SDValue TSC = CurDAG->getNode(
          ISD::OR,
          SDL_,
          MVT::i64,
          LO,
          CurDAG->getNode(
              ISD::SHL,
              SDL_,
              MVT::i64, HI,
              CurDAG->getConstant(32, SDL_, MVT::i8)
          )
      );
      Export(inst, TSC);
      CurDAG->setRoot(HI.getValue(1));
      return;
    }
    case Type::I128: {
      llvm_unreachable("not implemented");
    }
    case Type::F32: case Type::F64: case Type::F80: {
      llvm_unreachable("not implemented");
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerFNStCw(const FNStCwInst *inst)
{
  auto *mmo = MF->getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile |
      llvm::MachineMemOperand::MOStore,
      2,
      llvm::Align(1),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDValue addr = GetValue(inst->GetAddr());
  SDValue Ops[] = { CurDAG->getRoot(), addr };
  CurDAG->setRoot(
      CurDAG->getMemIntrinsicNode(
          X86ISD::FNSTCW16m,
          SDL_,
          CurDAG->getVTList(MVT::Other),
          Ops,
          MVT::i16,
          mmo
      )
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerFLdCw(const FLdCwInst *inst)
{
  auto *mmo = MF->getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile |
      llvm::MachineMemOperand::MOLoad,
      2,
      llvm::Align(1),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDValue addr = GetValue(inst->GetAddr());
  SDValue Ops[] = { CurDAG->getRoot(), addr };
  CurDAG->setRoot(
      CurDAG->getMemIntrinsicNode(
          X86ISD::FLDCW16m,
          SDL_,
          CurDAG->getVTList(MVT::Other),
          Ops,
          MVT::i16,
          mmo
      )
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerArgs()
{
  for (auto &argLoc : GetX86CallLowering().args()) {
    const llvm::TargetRegisterClass *regClass;
    MVT regType;
    unsigned size;
    switch (argLoc.ArgType) {
      case Type::I8:{
        regType = MVT::i8;
        regClass = &X86::GR8RegClass;
        size = 1;
        break;
      }
      case Type::I16:{
        regType = MVT::i16;
        regClass = &X86::GR16RegClass;
        size = 2;
        break;
      }
      case Type::I32: {
        regType = MVT::i32;
        regClass = &X86::GR32RegClass;
        size = 4;
        break;
      }
      case Type::I64: {
        regType = MVT::i64;
        regClass = &X86::GR64RegClass;
        size = 8;
        break;
      }
      case Type::I128: {
        Error(func_, "Invalid argument to call.");
      }
      case Type::F32: {
        regType = MVT::f32;
        regClass = &X86::FR32RegClass;
        size = 4;
        break;
      }
      case Type::F64: {
        regType = MVT::f64;
        regClass = &X86::FR64RegClass;
        size = 8;
        break;
      }
      case Type::F80: {
        regType = MVT::f80;
        regClass = &X86::RFP80RegClass;
        size = 10;
        break;
      }
    }

    SDValue arg;
    switch (argLoc.Kind) {
      case X86Call::Loc::Kind::REG: {
        unsigned Reg = MF->addLiveIn(argLoc.Reg, regClass);
        arg = CurDAG->getCopyFromReg(CurDAG->getEntryNode(), SDL_, Reg, regType);
        break;
      }
      case X86Call::Loc::Kind::STK: {
        llvm::MachineFrameInfo &MFI = MF->getFrameInfo();
        int index = MFI.CreateFixedObject(size, argLoc.Idx, true);

        args_[argLoc.Index] = index;

        arg = CurDAG->getLoad(
            regType,
            SDL_,
            CurDAG->getEntryNode(),
            CurDAG->getFrameIndex(index, GetPtrTy()),
            llvm::MachinePointerInfo::getFixedStack(
                CurDAG->getMachineFunction(),
                index
            )
        );
        break;
      }
    }

    for (const auto &block : *func_) {
      for (const auto &inst : block) {
        if (!inst.Is(Inst::Kind::ARG)) {
          continue;
        }
        auto &argInst = static_cast<const ArgInst &>(inst);
        if (argInst.GetIdx() == argLoc.Index) {
          Export(&argInst, arg);
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerVASetup()
{
  auto &ci = GetX86CallLowering();
  llvm::MachineFrameInfo &MFI = MF->getFrameInfo();
  auto ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());
  SDValue chain = CurDAG->getRoot();

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
    case CallingConv::CAML_GC:
    case CallingConv::CAML_RAISE: {
      Error(func_, "vararg call not supported");
    }
  }

  int index = MFI.CreateFixedObject(1, stackSize, false);
  FuncInfo_->setVarArgsFrameIndex(index);

  // Copy all unused regs to be pushed on the stack into vregs.
  llvm::SmallVector<SDValue, 6> liveGPRs;
  llvm::SmallVector<SDValue, 8> liveXMMs;
  SDValue alReg;

  for (unsigned reg : ci.GetUnusedGPRs()) {
    unsigned vreg = MF->addLiveIn(reg, &X86::GR64RegClass);
    liveGPRs.push_back(CurDAG->getCopyFromReg(chain, SDL_, vreg, MVT::i64));
  }

  for (unsigned reg : ci.GetUnusedXMMs()) {
    if (!alReg) {
      unsigned vreg = MF->addLiveIn(X86::AL, &X86::GR8RegClass);
      alReg = CurDAG->getCopyFromReg(chain, SDL_, vreg, MVT::i8);
    }
    unsigned vreg = MF->addLiveIn(reg, &X86::VR128RegClass);
    liveXMMs.push_back(CurDAG->getCopyFromReg(chain, SDL_, vreg, MVT::v4f32));
  }

  // Save the indices to be stored in __va_list_tag
  unsigned numGPRs = ci.GetUnusedGPRs().size() + ci.GetUsedGPRs().size();
  unsigned numXMMs = ci.GetUnusedXMMs().size() + ci.GetUsedXMMs().size();
  FuncInfo_->setVarArgsGPOffset(ci.GetUsedGPRs().size() * 8);
  FuncInfo_->setVarArgsFPOffset(numGPRs * 8 + ci.GetUsedXMMs().size() * 16);
  FuncInfo_->setRegSaveFrameIndex(MFI.CreateStackObject(
      numGPRs * 8 + numXMMs * 16,
      llvm::Align(16),
      false
  ));

  llvm::SmallVector<SDValue, 8> storeOps;
  SDValue frameIdx = CurDAG->getFrameIndex(
      FuncInfo_->getRegSaveFrameIndex(),
      ptrTy
  );

  // Store the unused GPR registers on the stack.
  unsigned gpOffset = FuncInfo_->getVarArgsGPOffset();
  for (SDValue val : liveGPRs) {
    SDValue valIdx = CurDAG->getNode(
        ISD::ADD,
        SDL_,
        ptrTy,
        frameIdx,
        CurDAG->getIntPtrConstant(gpOffset, SDL_)
    );
    storeOps.push_back(CurDAG->getStore(
        val.getValue(1),
        SDL_,
        val,
        valIdx,
        llvm::MachinePointerInfo::getFixedStack(
            CurDAG->getMachineFunction(),
            FuncInfo_->getRegSaveFrameIndex(),
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
    ops.push_back(CurDAG->getIntPtrConstant(
        FuncInfo_->getRegSaveFrameIndex(), SDL_)
    );
    ops.push_back(CurDAG->getIntPtrConstant(
        FuncInfo_->getVarArgsFPOffset(), SDL_)
    );
    ops.insert(ops.end(), liveXMMs.begin(), liveXMMs.end());
    storeOps.push_back(CurDAG->getNode(
        X86ISD::VASTART_SAVE_XMM_REGS,
        SDL_,
        MVT::Other,
        ops
    ));
  }

  if (!storeOps.empty()) {
    chain = CurDAG->getNode(ISD::TokenFactor, SDL_, MVT::Other, storeOps);
  }

  CurDAG->setRoot(chain);
}

// -----------------------------------------------------------------------------
SDValue X86ISel::LowerGetFS()
{
  auto *RegInfo = &MF->getRegInfo();
  const llvm::TargetLowering &TLI = GetTargetLowering();

  // Allocate a register to load FS to.
  auto reg = RegInfo->createVirtualRegister(TLI.getRegClassFor(MVT::i64));

  // Set up the inline assembly node.
  llvm::SmallVector<SDValue, 7> ops;
  ops.push_back(CurDAG->getRoot());
  ops.push_back(CurDAG->getTargetExternalSymbol(
      "mov %fs:0,$0",
      TLI.getProgramPointerTy(CurDAG->getDataLayout())
  ));
  ops.push_back(CurDAG->getMDNode(nullptr));
  ops.push_back(CurDAG->getTargetConstant(
      0,
      SDL_,
      TLI.getPointerTy(CurDAG->getDataLayout())
  ));

  // Register the output.
  {
    unsigned flag = llvm::InlineAsm::getFlagWord(
        llvm::InlineAsm::Kind_RegDef, 1
    );
    const auto *RC = RegInfo->getRegClass(reg);
    flag = llvm::InlineAsm::getFlagWordForRegClass(flag, RC->getID());
    ops.push_back(CurDAG->getTargetConstant(flag, SDL_, MVT::i32));
    ops.push_back(CurDAG->getRegister(reg, MVT::i64));
  }

  // Register clobbers.
  {
    unsigned flag = llvm::InlineAsm::getFlagWord(
        llvm::InlineAsm::Kind_Clobber, 1
    );
    ops.push_back(CurDAG->getTargetConstant(flag, SDL_, MVT::i32));
    ops.push_back(CurDAG->getRegister(X86::DF, MVT::i32));
    ops.push_back(CurDAG->getTargetConstant(flag, SDL_, MVT::i32));
    ops.push_back(CurDAG->getRegister(X86::FPSW, MVT::i16));
    ops.push_back(CurDAG->getTargetConstant(flag, SDL_, MVT::i32));
    ops.push_back(CurDAG->getRegister(X86::EFLAGS, MVT::i32));
  }

  // Create the inlineasm node.
  SDValue node = CurDAG->getNode(
      ISD::INLINEASM,
      SDL_,
      CurDAG->getVTList(MVT::Other, MVT::Glue),
      ops
  );
  SDValue chain = node.getValue(0);
  SDValue glue = node.getValue(1);

  // Copy out the vreg.
  auto copy = CurDAG->getCopyFromReg(
      chain,
      SDL_,
      reg,
      MVT::i64,
      glue
  );
  CurDAG->setRoot(copy.getValue(1));
  return copy.getValue(0);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSetRSP(SDValue value)
{
  CurDAG->setRoot(CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      X86::RSP,
      value
  ));
}

// -----------------------------------------------------------------------------
SDValue X86ISel::LoadReg(ConstantReg::Kind reg)
{
  auto copyFrom = [this](auto reg) {
    unsigned vreg = MF->addLiveIn(reg, &X86::GR64RegClass);
    auto copy = CurDAG->getCopyFromReg(
        CurDAG->getRoot(),
        SDL_,
        vreg,
        MVT::i64
    );
    CurDAG->setRoot(copy.getValue(1));
    return copy.getValue(0);
  };

  switch (reg) {
    // X86 architectural registers.
    case ConstantReg::Kind::RAX: return copyFrom(X86::RAX);
    case ConstantReg::Kind::RBX: return copyFrom(X86::RBX);
    case ConstantReg::Kind::RCX: return copyFrom(X86::RCX);
    case ConstantReg::Kind::RDX: return copyFrom(X86::RDX);
    case ConstantReg::Kind::RSI: return copyFrom(X86::RSI);
    case ConstantReg::Kind::RDI: return copyFrom(X86::RDI);
    case ConstantReg::Kind::RBP: return copyFrom(X86::RBP);
    case ConstantReg::Kind::R8:  return copyFrom(X86::R8);
    case ConstantReg::Kind::R9:  return copyFrom(X86::R9);
    case ConstantReg::Kind::R10: return copyFrom(X86::R10);
    case ConstantReg::Kind::R11: return copyFrom(X86::R11);
    case ConstantReg::Kind::R12: return copyFrom(X86::R12);
    case ConstantReg::Kind::R13: return copyFrom(X86::R13);
    case ConstantReg::Kind::R14: return copyFrom(X86::R14);
    case ConstantReg::Kind::R15: return copyFrom(X86::R15);
    // Thread pointer.
    case ConstantReg::Kind::FS:  return LowerGetFS();
    // Program counter.
    case ConstantReg::Kind::PC: {
      auto &MMI = MF->getMMI();
      auto *label = MMI.getContext().createTempSymbol();
      CurDAG->setRoot(CurDAG->getEHLabel(SDL_, CurDAG->getRoot(), label));
      return CurDAG->getNode(
          X86ISD::WrapperRIP,
          SDL_,
          MVT::i64,
          CurDAG->getMCSymbol(label, MVT::i64)
      );
    }
    // Stack pointer.
    case ConstantReg::Kind::RSP: {
      return CurDAG->getNode(ISD::STACKSAVE, SDL_, MVT::i64, CurDAG->getRoot());
    }
    // Return address.
    case ConstantReg::Kind::RET_ADDR: {
      return CurDAG->getNode(
          ISD::RETURNADDR,
          SDL_,
          MVT::i64,
          CurDAG->getTargetConstant(0, SDL_, MVT::i64)
      );
    }
    // Frame address.
    case ConstantReg::Kind::FRAME_ADDR: {
      MF->getFrameInfo().setReturnAddressIsTaken(true);

      if (frameIndex_ == 0) {
        frameIndex_ = MF->getFrameInfo().CreateFixedObject(8, 0, false);
      }

      return CurDAG->getFrameIndex(frameIndex_, MVT::i64);
    }
  }
  llvm_unreachable("invalid register kind");
}

// -----------------------------------------------------------------------------
llvm::SDValue X86ISel::LowerGlobal(const Global *val, int64_t offset)
{
  if (offset == 0) {
    return LowerGlobal(val);
  } else {
    return CurDAG->getNode(
        ISD::ADD,
        SDL_,
        GetPtrTy(),
        LowerGlobal(val),
        CurDAG->getConstant(offset, SDL_, GetPtrTy())
    );
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue X86ISel::LowerGlobal(const Global *val)
{
  const std::string_view name = val->GetName();
  const auto ptrTy = GetPtrTy();

  switch (val->GetKind()) {
    case Global::Kind::BLOCK: {
      auto *block = static_cast<const Block *>(val);
      if (auto *MBB = blocks_[block]) {
        auto *BB = const_cast<llvm::BasicBlock *>(MBB->getBasicBlock());
        auto *BA = llvm::BlockAddress::get(F_, BB);
        return CurDAG->getBlockAddress(BA, ptrTy);
      } else {
        llvm::report_fatal_error("Unknown block '" + std::string(name) + "'");
      }
    }
    case Global::Kind::FUNC:
    case Global::Kind::ATOM: {
      // Atom reference - need indirection for shared objects.
      auto *GV = M_->getNamedValue(name.data());
      if (!GV) {
        llvm::report_fatal_error("Unknown symbol '" + std::string(name) + "'");
        break;
      }

      if (shared_ && !val->IsHidden()) {
        SDValue addr = CurDAG->getTargetGlobalAddress(
            GV,
            SDL_,
            ptrTy,
            0,
            llvm::X86II::MO_GOTPCREL
        );

        SDValue addrRIP = CurDAG->getNode(
            X86ISD::WrapperRIP,
            SDL_,
            ptrTy,
            addr
        );

        return CurDAG->getLoad(
            ptrTy,
            SDL_,
            CurDAG->getEntryNode(),
            addrRIP,
            llvm::MachinePointerInfo::getGOT(CurDAG->getMachineFunction())
        );
      } else {
        return CurDAG->getNode(
            X86ISD::WrapperRIP,
            SDL_,
            ptrTy, CurDAG->getTargetGlobalAddress(
                GV,
                SDL_,
                ptrTy,
                0,
                llvm::X86II::MO_NO_FLAG
            )
        );
      }
    }
    case Global::Kind::EXTERN: {
      // Extern reference.
      auto *GV = M_->getNamedValue(name.data());
      if (!GV) {
        llvm::report_fatal_error("Unknown extern '" + std::string(name) + "'");
      }

      auto *ext = static_cast<const Extern *>(val);
      if (ext->GetSection() == ".text") {
        // Text-to-text references can be RIP-relative.
        return CurDAG->getNode(
            X86ISD::WrapperRIP,
            SDL_,
            ptrTy, CurDAG->getTargetGlobalAddress(
                GV,
                SDL_,
                ptrTy,
                0,
                llvm::X86II::MO_NO_FLAG
            )
        );
      } else {
         SDValue addr = CurDAG->getTargetGlobalAddress(
            GV,
            SDL_,
            ptrTy,
            0,
            llvm::X86II::MO_GOTPCREL
        );

        SDValue addrRIP = CurDAG->getNode(
            X86ISD::WrapperRIP,
            SDL_,
            ptrTy,
            addr
        );

        return CurDAG->getLoad(
            ptrTy,
            SDL_,
            CurDAG->getEntryNode(),
            addrRIP,
            llvm::MachinePointerInfo::getGOT(CurDAG->getMachineFunction())
        );
      }
    }
  }
  llvm_unreachable("invalid global type");
}

// -----------------------------------------------------------------------------
llvm::ScheduleDAGSDNodes *X86ISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}

// -----------------------------------------------------------------------------
template<typename T>
void X86ISel::LowerCallSite(SDValue chain, const CallSite<T> *call)
{
  const Block *block = call->getParent();
  const Func *func = block->getParent();
  auto ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();

  // Analyse the arguments, finding registers for them.
  bool isVarArg = call->GetNumArgs() > call->GetNumFixedArgs();
  bool isTailCall = call->Is(Inst::Kind::TCALL) || call->Is(Inst::Kind::TINVOKE);
  bool isInvoke = call->Is(Inst::Kind::INVOKE) || call->Is(Inst::Kind::TINVOKE);
  bool wasTailCall = isTailCall;
  X86Call locs(call, isVarArg, isTailCall);

  // Find the number of bytes allocated to hold arguments.
  unsigned stackSize = locs.GetFrameSize();

  // Compute the stack difference for tail calls.
  int fpDiff = 0;
  if (isTailCall) {
    X86Call callee(func);
    int bytesToPop;
    switch (func->GetCallingConv()) {
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
      case CallingConv::CAML_GC:
      case CallingConv::CAML_RAISE: {
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

  // Determine whether the call site need an OCaml frame.
  const bool hasFrame = call->template HasAnnot<CamlFrame>();

  // Calls from OCaml to C need to go through a trampoline.
  bool needsTrampoline = false;
  if (func->GetCallingConv() == CallingConv::CAML) {
    switch (call->GetCallingConv()) {
      case CallingConv::C:
        needsTrampoline = hasFrame;
        break;
      case CallingConv::SETJMP:
      case CallingConv::CAML:
      case CallingConv::CAML_ALLOC:
      case CallingConv::CAML_GC:
      case CallingConv::CAML_RAISE:
        break;
    }
  }

  // Find the register mask, based on the calling convention.
  unsigned cc;
  {
    using namespace llvm::CallingConv;
    if (needsTrampoline) {
      cc = LLIR_CAML_EXT;
    } else {
      switch (call->GetCallingConv()) {
        case CallingConv::C:          cc = C;               break;
        case CallingConv::CAML:       cc = LLIR_CAML;       break;
        case CallingConv::CAML_ALLOC: cc = LLIR_CAML_ALLOC; break;
        case CallingConv::CAML_GC:    cc = LLIR_CAML_GC;    break;
        case CallingConv::CAML_RAISE: cc = LLIR_CAML_RAISE; break;
        case CallingConv::SETJMP:     cc = LLIR_SETJMP;     break;
      }
    }
  }
  const uint32_t *mask = mask = TRI_->getCallPreservedMask(*MF, cc);

  // Instruction bundle starting the call.
  chain = CurDAG->getCALLSEQ_START(chain, stackSize, 0, SDL_);

  // Generate a GC_FRAME before the call, if needed.
  std::vector<std::pair<const Inst *, SDValue>> frameExport;
  if (hasFrame && func->GetCallingConv() == CallingConv::C) {
    SDValue frameOps[] = { chain };
    auto *symbol = MMI.getContext().createTempSymbol();
    chain = CurDAG->getGCFrame(SDL_, ISD::ROOT, frameOps, symbol);
  } else if (hasFrame && !isTailCall) {
    const auto *frame =  call->template GetAnnot<CamlFrame>();

    // Find the registers live across.
    frameExport = GetFrameExport(call);

    // Allocate a reg mask which does not block the return register.
    uint32_t *frameMask = MF->allocateRegMask();
    unsigned maskSize = llvm::MachineOperand::getRegMaskSize(TRI_->getNumRegs());
    memcpy(frameMask, mask, sizeof(frameMask[0]) * maskSize);

    if (wasTailCall || !call->use_empty()) {
      if (auto retTy = call->GetType()) {
        // Find the physical reg where the return value is stored.
        unsigned retReg;
        switch (*retTy) {
          case Type::I8:  retReg = X86::AL;   break;
          case Type::I16: retReg = X86::AX;   break;
          case Type::I32: retReg = X86::EAX;  break;
          case Type::I64: retReg = X86::RAX;  break;
          case Type::F32: retReg = X86::XMM0; break;
          case Type::F64: retReg = X86::XMM0; break;
          case Type::F80: retReg = X86::FP0;  break;
          case Type::I128: {
            Error(call, "unsupported return value type");
          }
        }

        // Clear all subregs.
        for (llvm::MCSubRegIterator SR(retReg, TRI_, true); SR.isValid(); ++SR) {
          frameMask[*SR / 32] |= 1u << (*SR % 32);
        }
      }
    }

    llvm::SmallVector<SDValue, 8> frameOps;
    frameOps.push_back(chain);
    frameOps.push_back(CurDAG->getRegisterMask(frameMask));
    for (auto &[inst, val] : frameExport) {
      frameOps.push_back(val);
    }
    for (auto alloc : frame->allocs()) {
      frameOps.push_back(CurDAG->getTargetConstant(alloc, SDL_, MVT::i64));
    }
    auto *symbol = MMI.getContext().createTempSymbol();
    frames_[symbol] = frame;
    chain = CurDAG->getGCFrame(SDL_, ISD::CALL, frameOps, symbol);
  }

  // Identify registers and stack locations holding the arguments.
  llvm::SmallVector<std::pair<unsigned, SDValue>, 8> regArgs;
  llvm::SmallVector<SDValue, 8> memOps;
  SDValue stackPtr;
  for (auto it = locs.arg_begin(); it != locs.arg_end(); ++it) {
    SDValue argument = GetValue(it->Value);
    switch (it->Kind) {
      case X86Call::Loc::Kind::REG: {
        regArgs.emplace_back(it->Reg, argument);
        break;
      }
      case X86Call::Loc::Kind::STK: {
        if (!stackPtr.getNode()) {
          stackPtr = CurDAG->getCopyFromReg(
              chain,
              SDL_,
              TRI_->getStackRegister(),
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

  if (isVarArg) {
    // If XMM regs are used, their count needs to be passed in AL.
    unsigned count = 0;
    for (auto arg : call->args()) {
      if (IsFloatType(arg->GetType(0))) {
        count = std::min(8u, count + 1);
      }
    }

    regArgs.push_back({ X86::AL, CurDAG->getConstant(count, SDL_, MVT::i8) });
  }

  if (isTailCall) {
    // Shuffle arguments on the stack.
    for (auto it = locs.arg_begin(); it != locs.arg_end(); ++it) {
      switch (it->Kind) {
        case X86Call::Loc::Kind::REG: {
          continue;
        }
        case X86Call::Loc::Kind::STK: {
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
    regArgs.emplace_back(X86::RAX, GetValue(call->GetCallee()));
    callee = CurDAG->getTargetGlobalAddress(
        trampoline_,
        SDL_,
        MVT::i64,
        0,
        llvm::X86II::MO_NO_FLAG
    );
  } else {
    if (auto *movInst = ::dyn_cast_or_null<MovInst>(call->GetCallee())) {
      auto *movArg = movInst->GetArg();
      switch (movArg->GetKind()) {
        case Value::Kind::INST:
          callee = GetValue(static_cast<const Inst *>(movArg));
          break;

        case Value::Kind::GLOBAL: {
          auto *movGlobal = static_cast<const Global *>(movArg);
          switch (movGlobal->GetKind()) {
            case Global::Kind::BLOCK: {
              llvm_unreachable("invalid call argument");
            }
            case Global::Kind::FUNC:
            case Global::Kind::ATOM:
            case Global::Kind::EXTERN: {
              const std::string_view name = movGlobal->GetName();
              if (auto *GV = M_->getNamedValue(name.data())) {
                callee = CurDAG->getTargetGlobalAddress(
                    GV,
                    SDL_,
                    MVT::i64,
                    0,
                    llvm::X86II::MO_NO_FLAG
                );
              } else {
                Error(call, "Unknown symbol '" + std::string(name) + "'");
              }
              break;
            }
          }
          break;
        }
        case Value::Kind::EXPR:
        case Value::Kind::CONST: {
          llvm_unreachable("invalid call argument");
        }
      }
    } else {
      callee = GetValue(call->GetCallee());
    }
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
  if (isTailCall) {
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
    ops.push_back(CurDAG->getConstant(fpDiff, SDL_, MVT::i32));
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
    CurDAG->setRoot(CurDAG->getNode(X86ISD::TC_RETURN, SDL_, nodeTypes, ops));
  } else {
    chain = CurDAG->getNode(X86ISD::CALL, SDL_, nodeTypes, ops);
    inFlag = chain.getValue(1);

    chain = CurDAG->getCALLSEQ_END(
        chain,
        CurDAG->getIntPtrConstant(stackSize, SDL_, true),
        CurDAG->getIntPtrConstant(0, SDL_, true),
        inFlag,
        SDL_
    );

    // Lower the return value.
    std::vector<SDValue> tailReturns;
    if (auto retTy = call->GetType()) {
      // Find the physical reg where the return value is stored.
      unsigned retReg;
      MVT retVT;
      switch (*retTy) {
        case Type::I8: {
          retReg = X86::AL;
          retVT = MVT::i8;
          break;
        }
        case Type::I16: {
          retReg = X86::AX;
          retVT = MVT::i16;
          break;
        }
        case Type::I32: {
          retReg = X86::EAX;
          retVT = MVT::i32;
          break;
        }
        case Type::I64: {
          retReg = X86::RAX;
          retVT = MVT::i64;
          break;
        }
        case Type::I128: {
          Error(call, "unsupported return value type");
        }
        case Type::F32: {
          retReg = X86::XMM0;
          retVT = MVT::f32;
          break;
        }
        case Type::F64: {
          retReg = X86::XMM0;
          retVT = MVT::f64;
          break;
        }
        case Type::F80: {
          retReg = X86::FP0;
          retVT = MVT::f80;
          break;
        }
      }

      if (wasTailCall || !isTailCall) {
        if (wasTailCall) {
          /// Copy the return value into a vreg.
          chain = CurDAG->getCopyFromReg(
              chain,
              SDL_,
              retReg,
              retVT,
              chain.getValue(1)
          ).getValue(1);

          /// If this was a tailcall, forward to return.
          tailReturns.push_back(chain.getValue(0));
        } else {
          // Regular call with a return which is used - expose it.
          if (!call->use_empty()) {
            chain = CurDAG->getCopyFromReg(
                chain,
                SDL_,
                retReg,
                retVT,
                chain.getValue(1)
            ).getValue(1);

            // Otherwise, expose the value.
            Export(call, chain.getValue(0));
          }

          // Ensure live values are not lifted before this point.
          if (!isInvoke) {
            for (auto &[inst, v] : frameExport) {
              chain = BreakVar(chain, inst, v);
            }
          }
        }
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
          X86ISD::RET_FLAG,
          SDL_,
          MVT::Other,
          returns
      );
    }

    CurDAG->setRoot(chain);
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSyscall(const SyscallInst *inst)
{
  static unsigned kRegs[] = {
      X86::RDI, X86::RSI, X86::RDX,
      X86::R10, X86::R8, X86::R9
  };

  llvm::SmallVector<SDValue, 7> ops;
  SDValue chain = CurDAG->getRoot();

  // Lower arguments.
  unsigned args = 0;
  {
    unsigned n = sizeof(kRegs) / sizeof(kRegs[0]);
    for (const Inst *arg : inst->args()) {
      if (args >= n) {
        Error(inst, "too many arguments to syscall");
      }

      SDValue value = GetValue(arg);
      if (arg->GetType(0) != Type::I64) {
        Error(inst, "invalid syscall argument");
      }
      ops.push_back(CurDAG->getRegister(kRegs[args], MVT::i64));
      chain = CurDAG->getCopyToReg(chain, SDL_, kRegs[args++], value);
    }
  }

  /// Lower to the syscall.
  {
    ops.push_back(CurDAG->getRegister(X86::RAX, MVT::i64));

    chain = CurDAG->getCopyToReg(
        chain,
        SDL_,
        X86::RAX,
        GetValue(inst->GetSyscall())
    );

    ops.push_back(chain);

    chain = SDValue(CurDAG->getMachineNode(
        X86::SYSCALL,
        SDL_,
        CurDAG->getVTList(MVT::Other, MVT::Glue),
        ops
    ), 0);
  }

  /// Copy the return value into a vreg and export it.
  {
    if (inst->GetType() != Type::I64) {
      Error(inst, "invalid syscall type");
    }

    chain = CurDAG->getCopyFromReg(
        chain,
        SDL_,
        X86::RAX,
        MVT::i64,
        chain.getValue(1)
    ).getValue(1);

    Export(inst, chain.getValue(0));
  }

  CurDAG->setRoot(chain);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSwitch(const SwitchInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();
  llvm::MachineFunction &MF = dag.getMachineFunction();
  const llvm::TargetLowering &TLI = GetTargetLowering();

  auto *sourceMBB = blocks_[inst->getParent()];

  std::vector<llvm::MachineBasicBlock*> branches;
  for (unsigned i = 0, n = inst->getNumSuccessors(); i < n; ++i) {
    auto *mbb = blocks_[inst->getSuccessor(i)];
    branches.push_back(mbb);
  }

  {
    llvm::DenseSet<llvm::MachineBasicBlock *> added;
    for (auto *mbb : branches) {
      if (added.insert(mbb).second) {
        sourceMBB->addSuccessor(mbb);
      }
    }
  }

  sourceMBB->normalizeSuccProbs();

  auto *jti = MF.getOrCreateJumpTableInfo(TLI.getJumpTableEncoding());
  int jumpTableId = jti->createJumpTableIndex(branches);
  auto ptrTy = TLI.getPointerTy(dag.getDataLayout());

  SDValue jt = dag.getTargetJumpTable(
      jumpTableId,
      ptrTy,
      llvm::X86II::MO_NO_FLAG
  );

  jt = dag.getNode(
      X86ISD::WrapperRIP,
      SDL_,
      ptrTy,
      jt
  );

  dag.setRoot(dag.getNode(
      ISD::BR_JT,
      SDL_,
      MVT::Other,
      GetExportRoot(),
      jt,
      GetValue(inst->GetIdx())
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerRaise(const RaiseInst *inst)
{
  auto *RegInfo = &MF->getRegInfo();
  const llvm::TargetLowering &TLI = GetTargetLowering();

  // Allocate a register to load FS to.
  auto pc = RegInfo->createVirtualRegister(TLI.getRegClassFor(MVT::i64));
  auto stk = RegInfo->createVirtualRegister(TLI.getRegClassFor(MVT::i64));

  // Copy in the new stack pointer and code pointer.
  SDValue stkNode = CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      stk,
      GetValue(inst->GetStack())
  );
  SDValue pcNode = CurDAG->getCopyToReg(
      stkNode,
      SDL_,
      pc,
      GetValue(inst->GetTarget()),
      stkNode
  );

  // Set up the inline assembly node.
  llvm::SmallVector<SDValue, 7> ops;
  ops.push_back(pcNode);
  ops.push_back(CurDAG->getTargetExternalSymbol(
      "mov $0, %rsp ; jmp *$1",
      TLI.getProgramPointerTy(CurDAG->getDataLayout())
  ));
  ops.push_back(CurDAG->getMDNode(nullptr));
  ops.push_back(CurDAG->getTargetConstant(
      llvm::InlineAsm::Extra_MayLoad | llvm::InlineAsm::Extra_MayStore,
      SDL_,
      TLI.getPointerTy(CurDAG->getDataLayout())
  ));

  // Register the input.
  {
    auto AddRegister = [&] (unsigned reg) {
      const auto *RC = RegInfo->getRegClass(reg);
      const unsigned flag = llvm::InlineAsm::getFlagWordForRegClass(
          llvm::InlineAsm::getFlagWord(llvm::InlineAsm::Kind_RegUse, 1),
          RC->getID()
      );
      ops.push_back(CurDAG->getTargetConstant(flag, SDL_, MVT::i32));
      ops.push_back(CurDAG->getRegister(reg, MVT::i64));
    };
    AddRegister(stk);
    AddRegister(pc);
  }

  // Register clobbers.
  {
    unsigned flag = llvm::InlineAsm::getFlagWord(
        llvm::InlineAsm::Kind_Clobber, 1
    );
    ops.push_back(CurDAG->getTargetConstant(flag, SDL_, MVT::i32));
    ops.push_back(CurDAG->getRegister(X86::DF, MVT::i32));
    ops.push_back(CurDAG->getTargetConstant(flag, SDL_, MVT::i32));
    ops.push_back(CurDAG->getRegister(X86::FPSW, MVT::i16));
    ops.push_back(CurDAG->getTargetConstant(flag, SDL_, MVT::i32));
    ops.push_back(CurDAG->getRegister(X86::EFLAGS, MVT::i32));
  }

  // Add the glue.
  ops.push_back(pcNode.getValue(1));

  // Create the inlineasm node.
  SDValue node = CurDAG->getNode(
      ISD::INLINEASM,
      SDL_,
      CurDAG->getVTList(MVT::Other, MVT::Glue),
      ops
  );
  CurDAG->setRoot(node);
}

// -----------------------------------------------------------------------------
llvm::SDValue X86ISel::BreakVar(SDValue chain, const Inst *inst, SDValue value)
{
  if (value->getOpcode() == ISD::GC_ARG) {
    return chain;
  }

  auto *RegInfo = &MF->getRegInfo();
  auto reg = RegInfo->createVirtualRegister(TLI->getRegClassFor(MVT::i64));
  chain = CurDAG->getCopyToReg(chain, SDL_, reg, value);
  chain = CurDAG->getCopyFromReg(
      chain,
      SDL_,
      reg,
      MVT::i64
  ).getValue(1);

  values_[inst] = chain.getValue(0);
  if (auto it = regs_.find(inst); it != regs_.end()) {
    if (auto jt = pendingExports_.find(it->second); jt != pendingExports_.end()) {
      jt->second = chain.getValue(0);
    }
  }

  return chain;
}

// -----------------------------------------------------------------------------
X86Call &X86ISel::GetX86CallLowering()
{
  if (!conv_ || conv_->first != func_) {
    conv_ = std::make_unique<std::pair<const Func *, X86Call>>(
        func_,
        X86Call{ func_ }
    );
  }
  return conv_->second;
}
