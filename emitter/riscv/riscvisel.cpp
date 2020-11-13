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
                  llvm::RISCVII::MO_None
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
void RISCVISel::LowerCallSite(SDValue chain, const CallSite *call)
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
    regArgs.emplace_back(RISCV::X16, GetValue(call->GetCallee()));
    callee = CurDAG->getTargetGlobalAddress(
        trampoline_,
        SDL_,
        MVT::i64,
        0,
        llvm::RISCVII::MO_None
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
        RISCVISD::TAIL,
        SDL_,
        nodeTypes,
        ops
    ));
  } else {
    chain = CurDAG->getNode(RISCVISD::CALL, SDL_, nodeTypes, ops);
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

    CurDAG->setRoot(chain);
  }
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
