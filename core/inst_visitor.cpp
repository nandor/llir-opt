// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/inst_visitor.h"


// -----------------------------------------------------------------------------
InstVisitor::~InstVisitor()
{
}

// -----------------------------------------------------------------------------
void InstVisitor::Dispatch(Inst *i)
{
  switch (i->GetKind()) {
    case Inst::Kind::CALL:     return VisitCall(static_cast<CallInst *>(i));
    case Inst::Kind::TCALL:    return VisitTailCall(static_cast<TailCallInst *>(i));
    case Inst::Kind::INVOKE:   return VisitInvoke(static_cast<InvokeInst *>(i));
    case Inst::Kind::TINVOKE:  return VisitTailInvoke(static_cast<TailInvokeInst *>(i));
    case Inst::Kind::RET:      return VisitReturn(static_cast<ReturnInst *>(i));
    case Inst::Kind::JCC:      return VisitJumpCond(static_cast<JumpCondInst *>(i));
    case Inst::Kind::JI:       return VisitJumpIndirect(static_cast<JumpIndirectInst *>(i));
    case Inst::Kind::JMP:      return VisitJump(static_cast<JumpInst *>(i));
    case Inst::Kind::SWITCH:   return VisitSwitch(static_cast<SwitchInst *>(i));
    case Inst::Kind::TRAP:     return VisitTrap(static_cast<TrapInst *>(i));
    case Inst::Kind::SYSCALL:  return VisitSyscall(static_cast<SyscallInst *>(i));
    case Inst::Kind::SET:      return VisitSet(static_cast<SetInst *>(i));
    case Inst::Kind::MOV:      return VisitMov(static_cast<MovInst *>(i));
    case Inst::Kind::LD:       return VisitLoad(static_cast<LoadInst *>(i));
    case Inst::Kind::ST:       return VisitStore(static_cast<StoreInst *>(i));
    case Inst::Kind::XCHG:     return VisitXchg(static_cast<XchgInst *>(i));
    case Inst::Kind::CMPXCHG:  return VisitCmpXchg(static_cast<CmpXchgInst *>(i));
    case Inst::Kind::VASTART:  return VisitVAStart(static_cast<VAStartInst *>(i));
    case Inst::Kind::ALLOCA:   return VisitAlloca(static_cast<AllocaInst *>(i));
    case Inst::Kind::ARG:      return VisitArg(static_cast<ArgInst *>(i));
    case Inst::Kind::FRAME:    return VisitFrame(static_cast<FrameInst *>(i));
    case Inst::Kind::UNDEF:    return VisitUndef(static_cast<UndefInst *>(i));
    case Inst::Kind::RDTSC:    return VisitRdtsc(static_cast<RdtscInst *>(i));
    case Inst::Kind::FNSTCW:   return VisitFNStCw(static_cast<FNStCwInst *>(i));
    case Inst::Kind::FLDCW:    return VisitFLdCw(static_cast<FLdCwInst *>(i));
    case Inst::Kind::SELECT:   return VisitSelect(static_cast<SelectInst *>(i));
    case Inst::Kind::ABS:      return VisitAbs(static_cast<AbsInst *>(i));
    case Inst::Kind::NEG:      return VisitNeg(static_cast<NegInst *>(i));
    case Inst::Kind::SQRT:     return VisitSqrt(static_cast<SqrtInst *>(i));
    case Inst::Kind::SIN:      return VisitSin(static_cast<SinInst *>(i));
    case Inst::Kind::COS:      return VisitCos(static_cast<CosInst *>(i));
    case Inst::Kind::SEXT:     return VisitSExt(static_cast<SExtInst *>(i));
    case Inst::Kind::ZEXT:     return VisitZExt(static_cast<ZExtInst *>(i));
    case Inst::Kind::FEXT:     return VisitFExt(static_cast<FExtInst *>(i));
    case Inst::Kind::XEXT:     return VisitXExt(static_cast<XExtInst *>(i));
    case Inst::Kind::TRUNC:    return VisitTrunc(static_cast<TruncInst *>(i));
    case Inst::Kind::EXP:      return VisitExp(static_cast<ExpInst *>(i));
    case Inst::Kind::EXP2:     return VisitExp2(static_cast<Exp2Inst *>(i));
    case Inst::Kind::LOG:      return VisitLog(static_cast<LogInst *>(i));
    case Inst::Kind::LOG2:     return VisitLog2(static_cast<Log2Inst *>(i));
    case Inst::Kind::LOG10:    return VisitLog10(static_cast<Log10Inst *>(i));
    case Inst::Kind::FCEIL:    return VisitFCeil(static_cast<FCeilInst *>(i));
    case Inst::Kind::FFLOOR:   return VisitFFloor(static_cast<FFloorInst *>(i));
    case Inst::Kind::POPCNT:   return VisitPopCount(static_cast<PopCountInst *>(i));
    case Inst::Kind::CLZ:      return VisitCLZ(static_cast<CLZInst *>(i));
    case Inst::Kind::CTZ:      return VisitCTZ(static_cast<CTZInst *>(i));
    case Inst::Kind::ADD:      return VisitAdd(static_cast<AddInst *>(i));
    case Inst::Kind::AND:      return VisitAnd(static_cast<AndInst *>(i));
    case Inst::Kind::CMP:      return VisitCmp(static_cast<CmpInst *>(i));
    case Inst::Kind::UDIV:     return VisitUDiv(static_cast<UDivInst *>(i));
    case Inst::Kind::UREM:     return VisitURem(static_cast<URemInst *>(i));
    case Inst::Kind::SDIV:     return VisitSDiv(static_cast<SDivInst *>(i));
    case Inst::Kind::SREM:     return VisitSRem(static_cast<SRemInst *>(i));
    case Inst::Kind::MUL:      return VisitMul(static_cast<MulInst *>(i));
    case Inst::Kind::OR:       return VisitOr(static_cast<OrInst *>(i));
    case Inst::Kind::ROTL:     return VisitRotl(static_cast<RotlInst *>(i));
    case Inst::Kind::ROTR:     return VisitRotr(static_cast<RotrInst *>(i));
    case Inst::Kind::SLL:      return VisitSll(static_cast<SllInst *>(i));
    case Inst::Kind::SRA:      return VisitSra(static_cast<SraInst *>(i));
    case Inst::Kind::SRL:      return VisitSrl(static_cast<SrlInst *>(i));
    case Inst::Kind::SUB:      return VisitSub(static_cast<SubInst *>(i));
    case Inst::Kind::XOR:      return VisitXor(static_cast<XorInst *>(i));
    case Inst::Kind::POW:      return VisitPow(static_cast<PowInst *>(i));
    case Inst::Kind::COPYSIGN: return VisitCopySign(static_cast<CopySignInst *>(i));
    case Inst::Kind::UADDO:    return VisitAddUO(static_cast<AddUOInst *>(i));
    case Inst::Kind::UMULO:    return VisitMulUO(static_cast<MulUOInst *>(i));
    case Inst::Kind::USUBO:    return VisitSubUO(static_cast<SubUOInst *>(i));
    case Inst::Kind::SADDO:    return VisitAddSO(static_cast<AddSOInst *>(i));
    case Inst::Kind::SMULO:    return VisitMulSO(static_cast<MulSOInst *>(i));
    case Inst::Kind::SSUBO:    return VisitSubSO(static_cast<SubSOInst *>(i));
    case Inst::Kind::PHI:      return VisitPhi(static_cast<PhiInst *>(i));
  }
  llvm_unreachable("invalid instruction kind");
}
