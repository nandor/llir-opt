// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/insts.h"



template<typename T>
class InstVisitor {
public:
  virtual ~InstVisitor() {}

  T Dispatch(Inst *i);

public:
  virtual T Visit(Inst *i) = 0;
  virtual T VisitUnary(UnaryInst *i) { return Visit(i); }
  virtual T VisitBinary(BinaryInst *i) { return Visit(i); }

public:
  virtual T VisitCall(CallInst *i) { return Visit(i); }
  virtual T VisitTailCall(TailCallInst *i) { return Visit(i); }
  virtual T VisitInvoke(InvokeInst *i) { return Visit(i); }
  virtual T VisitReturn(ReturnInst *i) { return Visit(i); }
  virtual T VisitJumpCond(JumpCondInst *i) { return Visit(i); }
  virtual T VisitRaise(RaiseInst *i) { return Visit(i); }
  virtual T VisitJump(JumpInst *i) { return Visit(i); }
  virtual T VisitSwitch(SwitchInst *i) { return Visit(i); }
  virtual T VisitTrap(TrapInst *i) { return Visit(i); }
  virtual T VisitSyscall(SyscallInst *i) { return Visit(i); }
  virtual T VisitClone(CloneInst *i) { return Visit(i); }
  virtual T VisitSet(SetInst *i) { return Visit(i); }
  virtual T VisitRdtsc(RdtscInst *i) { return Visit(i); }
  virtual T VisitMov(MovInst *i) { return Visit(i); }
  virtual T VisitLoad(LoadInst *i) { return Visit(i); }
  virtual T VisitStore(StoreInst *i) { return Visit(i); }
  virtual T VisitVAStart(VAStartInst *i) { return Visit(i); }
  virtual T VisitAlloca(AllocaInst *i) { return Visit(i); }
  virtual T VisitArg(ArgInst *i) { return Visit(i); }
  virtual T VisitFrame(FrameInst *i) { return Visit(i); }
  virtual T VisitUndef(UndefInst *i) { return Visit(i); }
  virtual T VisitSelect(SelectInst *i) { return Visit(i); }
  virtual T VisitAbs(AbsInst *i) { return VisitUnary(i); }
  virtual T VisitNeg(NegInst *i) { return VisitUnary(i); }
  virtual T VisitSqrt(SqrtInst *i) { return VisitUnary(i); }
  virtual T VisitSin(SinInst *i) { return VisitUnary(i); }
  virtual T VisitCos(CosInst *i) { return VisitUnary(i); }
  virtual T VisitSExt(SExtInst *i) { return VisitUnary(i); }
  virtual T VisitZExt(ZExtInst *i) { return VisitUnary(i); }
  virtual T VisitFExt(FExtInst *i) { return VisitUnary(i); }
  virtual T VisitXExt(XExtInst *i) { return VisitUnary(i); }
  virtual T VisitTrunc(TruncInst *i) { return VisitUnary(i); }
  virtual T VisitExp(ExpInst *i) { return VisitUnary(i); }
  virtual T VisitExp2(Exp2Inst *i) { return VisitUnary(i); }
  virtual T VisitLog(LogInst *i) { return VisitUnary(i); }
  virtual T VisitLog2(Log2Inst *i) { return VisitUnary(i); }
  virtual T VisitLog10(Log10Inst *i) { return VisitUnary(i); }
  virtual T VisitFCeil(FCeilInst *i) { return VisitUnary(i); }
  virtual T VisitFFloor(FFloorInst *i) { return VisitUnary(i); }
  virtual T VisitPopCount(PopCountInst *i) { return VisitUnary(i); }
  virtual T VisitBSwap(BSwapInst *i) { return VisitUnary(i); }
  virtual T VisitCLZ(CLZInst *i) { return VisitUnary(i); }
  virtual T VisitCTZ(CTZInst *i) { return VisitUnary(i); }
  virtual T VisitAdd(AddInst *i) { return VisitBinary(i); }
  virtual T VisitAnd(AndInst *i) { return VisitBinary(i); }
  virtual T VisitCmp(CmpInst *i) { return VisitBinary(i); }
  virtual T VisitUDiv(UDivInst *i) { return VisitBinary(i); }
  virtual T VisitURem(URemInst *i) { return VisitBinary(i); }
  virtual T VisitSDiv(SDivInst *i) { return VisitBinary(i); }
  virtual T VisitSRem(SRemInst *i) { return VisitBinary(i); }
  virtual T VisitMul(MulInst *i) { return VisitBinary(i); }
  virtual T VisitOr(OrInst *i) { return VisitBinary(i); }
  virtual T VisitRotl(RotlInst *i) { return VisitBinary(i); }
  virtual T VisitRotr(RotrInst *i) { return VisitBinary(i); }
  virtual T VisitSll(SllInst *i) { return VisitBinary(i); }
  virtual T VisitSra(SraInst *i) { return VisitBinary(i); }
  virtual T VisitSrl(SrlInst *i) { return VisitBinary(i); }
  virtual T VisitSub(SubInst *i) { return VisitBinary(i); }
  virtual T VisitXor(XorInst *i) { return VisitBinary(i); }
  virtual T VisitPow(PowInst *i) { return VisitBinary(i); }
  virtual T VisitCopySign(CopySignInst *i) { return VisitBinary(i); }
  virtual T VisitAddUO(AddUOInst *i) { return VisitBinary(i); }
  virtual T VisitMulUO(MulUOInst *i) { return VisitBinary(i); }
  virtual T VisitSubUO(SubUOInst *i) { return VisitBinary(i); }
  virtual T VisitAddSO(AddSOInst *i) { return VisitBinary(i); }
  virtual T VisitMulSO(MulSOInst *i) { return VisitBinary(i); }
  virtual T VisitSubSO(SubSOInst *i) { return VisitBinary(i); }
  virtual T VisitPhi(PhiInst *i) { return Visit(i); }
  virtual T VisitX86_Xchg(X86_XchgInst *i) { return Visit(i); }
  virtual T VisitX86_CmpXchg(X86_CmpXchgInst *i) { return Visit(i); }
  virtual T VisitX86_FnClEx(X86_FnClExInst *i) { return Visit(i); }
  virtual T VisitX86_FPUControlInst(X86_FPUControlInst *i) { return Visit(i); }
  virtual T VisitX86_FnStSw(X86_FnStSwInst *i) { return VisitX86_FPUControlInst(i); }
  virtual T VisitX86_FnStCw(X86_FnStCwInst *i) { return VisitX86_FPUControlInst(i); }
  virtual T VisitX86_FnStEnv(X86_FnStEnvInst *i) { return VisitX86_FPUControlInst(i); }
  virtual T VisitX86_FLdCw(X86_FLdCwInst *i) { return VisitX86_FPUControlInst(i); }
  virtual T VisitX86_FLdEnv(X86_FLdEnvInst *i) { return VisitX86_FPUControlInst(i); }
  virtual T VisitX86_LdmXCSR(X86_LdmXCSRInst *i) { return VisitX86_FPUControlInst(i); }
  virtual T VisitX86_StmXCSR(X86_StmXCSRInst *i) { return VisitX86_FPUControlInst(i); }
  virtual T VisitAArch64_LL(AArch64_LL *i) { return Visit(i); }
  virtual T VisitAArch64_SC(AArch64_SC *i) { return Visit(i); }
  virtual T VisitAArch64_DMB(AArch64_DMB *i) { return Visit(i); }
};

// -----------------------------------------------------------------------------
template <typename T>
T InstVisitor<T>::Dispatch(Inst *i)
{
  switch (i->GetKind()) {
    case Inst::Kind::CALL:        return VisitCall(static_cast<CallInst *>(i));
    case Inst::Kind::TCALL:       return VisitTailCall(static_cast<TailCallInst *>(i));
    case Inst::Kind::INVOKE:      return VisitInvoke(static_cast<InvokeInst *>(i));
    case Inst::Kind::RET:         return VisitReturn(static_cast<ReturnInst *>(i));
    case Inst::Kind::JCC:         return VisitJumpCond(static_cast<JumpCondInst *>(i));
    case Inst::Kind::RAISE:       return VisitRaise(static_cast<RaiseInst *>(i));
    case Inst::Kind::JMP:         return VisitJump(static_cast<JumpInst *>(i));
    case Inst::Kind::SWITCH:      return VisitSwitch(static_cast<SwitchInst *>(i));
    case Inst::Kind::TRAP:        return VisitTrap(static_cast<TrapInst *>(i));
    case Inst::Kind::SYSCALL:     return VisitSyscall(static_cast<SyscallInst *>(i));
    case Inst::Kind::CLONE:       return VisitClone(static_cast<CloneInst *>(i));
    case Inst::Kind::SET:         return VisitSet(static_cast<SetInst *>(i));
    case Inst::Kind::RDTSC:       return VisitRdtsc(static_cast<RdtscInst *>(i));
    case Inst::Kind::MOV:         return VisitMov(static_cast<MovInst *>(i));
    case Inst::Kind::LD:          return VisitLoad(static_cast<LoadInst *>(i));
    case Inst::Kind::ST:          return VisitStore(static_cast<StoreInst *>(i));
    case Inst::Kind::VASTART:     return VisitVAStart(static_cast<VAStartInst *>(i));
    case Inst::Kind::ALLOCA:      return VisitAlloca(static_cast<AllocaInst *>(i));
    case Inst::Kind::ARG:         return VisitArg(static_cast<ArgInst *>(i));
    case Inst::Kind::FRAME:       return VisitFrame(static_cast<FrameInst *>(i));
    case Inst::Kind::UNDEF:       return VisitUndef(static_cast<UndefInst *>(i));
    case Inst::Kind::SELECT:      return VisitSelect(static_cast<SelectInst *>(i));
    case Inst::Kind::ABS:         return VisitAbs(static_cast<AbsInst *>(i));
    case Inst::Kind::NEG:         return VisitNeg(static_cast<NegInst *>(i));
    case Inst::Kind::SQRT:        return VisitSqrt(static_cast<SqrtInst *>(i));
    case Inst::Kind::SIN:         return VisitSin(static_cast<SinInst *>(i));
    case Inst::Kind::COS:         return VisitCos(static_cast<CosInst *>(i));
    case Inst::Kind::SEXT:        return VisitSExt(static_cast<SExtInst *>(i));
    case Inst::Kind::ZEXT:        return VisitZExt(static_cast<ZExtInst *>(i));
    case Inst::Kind::FEXT:        return VisitFExt(static_cast<FExtInst *>(i));
    case Inst::Kind::XEXT:        return VisitXExt(static_cast<XExtInst *>(i));
    case Inst::Kind::TRUNC:       return VisitTrunc(static_cast<TruncInst *>(i));
    case Inst::Kind::EXP:         return VisitExp(static_cast<ExpInst *>(i));
    case Inst::Kind::EXP2:        return VisitExp2(static_cast<Exp2Inst *>(i));
    case Inst::Kind::LOG:         return VisitLog(static_cast<LogInst *>(i));
    case Inst::Kind::LOG2:        return VisitLog2(static_cast<Log2Inst *>(i));
    case Inst::Kind::LOG10:       return VisitLog10(static_cast<Log10Inst *>(i));
    case Inst::Kind::FCEIL:       return VisitFCeil(static_cast<FCeilInst *>(i));
    case Inst::Kind::FFLOOR:      return VisitFFloor(static_cast<FFloorInst *>(i));
    case Inst::Kind::POPCNT:      return VisitPopCount(static_cast<PopCountInst *>(i));
    case Inst::Kind::BSWAP:       return VisitBSwap(static_cast<BSwapInst *>(i));
    case Inst::Kind::CLZ:         return VisitCLZ(static_cast<CLZInst *>(i));
    case Inst::Kind::CTZ:         return VisitCTZ(static_cast<CTZInst *>(i));
    case Inst::Kind::ADD:         return VisitAdd(static_cast<AddInst *>(i));
    case Inst::Kind::AND:         return VisitAnd(static_cast<AndInst *>(i));
    case Inst::Kind::CMP:         return VisitCmp(static_cast<CmpInst *>(i));
    case Inst::Kind::UDIV:        return VisitUDiv(static_cast<UDivInst *>(i));
    case Inst::Kind::UREM:        return VisitURem(static_cast<URemInst *>(i));
    case Inst::Kind::SDIV:        return VisitSDiv(static_cast<SDivInst *>(i));
    case Inst::Kind::SREM:        return VisitSRem(static_cast<SRemInst *>(i));
    case Inst::Kind::MUL:         return VisitMul(static_cast<MulInst *>(i));
    case Inst::Kind::OR:          return VisitOr(static_cast<OrInst *>(i));
    case Inst::Kind::ROTL:        return VisitRotl(static_cast<RotlInst *>(i));
    case Inst::Kind::ROTR:        return VisitRotr(static_cast<RotrInst *>(i));
    case Inst::Kind::SLL:         return VisitSll(static_cast<SllInst *>(i));
    case Inst::Kind::SRA:         return VisitSra(static_cast<SraInst *>(i));
    case Inst::Kind::SRL:         return VisitSrl(static_cast<SrlInst *>(i));
    case Inst::Kind::SUB:         return VisitSub(static_cast<SubInst *>(i));
    case Inst::Kind::XOR:         return VisitXor(static_cast<XorInst *>(i));
    case Inst::Kind::POW:         return VisitPow(static_cast<PowInst *>(i));
    case Inst::Kind::COPYSIGN:    return VisitCopySign(static_cast<CopySignInst *>(i));
    case Inst::Kind::UADDO:       return VisitAddUO(static_cast<AddUOInst *>(i));
    case Inst::Kind::UMULO:       return VisitMulUO(static_cast<MulUOInst *>(i));
    case Inst::Kind::USUBO:       return VisitSubUO(static_cast<SubUOInst *>(i));
    case Inst::Kind::SADDO:       return VisitAddSO(static_cast<AddSOInst *>(i));
    case Inst::Kind::SMULO:       return VisitMulSO(static_cast<MulSOInst *>(i));
    case Inst::Kind::SSUBO:       return VisitSubSO(static_cast<SubSOInst *>(i));
    case Inst::Kind::PHI:         return VisitPhi(static_cast<PhiInst *>(i));
    case Inst::Kind::X86_XCHG:    return VisitX86_Xchg(static_cast<X86_XchgInst *>(i));
    case Inst::Kind::X86_CMPXCHG: return VisitX86_CmpXchg(static_cast<X86_CmpXchgInst *>(i));
    case Inst::Kind::X86_FNSTCW:  return VisitX86_FnStCw(static_cast<X86_FnStCwInst *>(i));
    case Inst::Kind::X86_FNSTSW:  return VisitX86_FnStSw(static_cast<X86_FnStSwInst *>(i));
    case Inst::Kind::X86_FNSTENV: return VisitX86_FnStEnv(static_cast<X86_FnStEnvInst *>(i));
    case Inst::Kind::X86_FLDCW:   return VisitX86_FLdCw(static_cast<X86_FLdCwInst *>(i));
    case Inst::Kind::X86_FLDENV:  return VisitX86_FLdEnv(static_cast<X86_FLdEnvInst *>(i));
    case Inst::Kind::X86_LDMXCSR: return VisitX86_LdmXCSR(static_cast<X86_LdmXCSRInst *>(i));
    case Inst::Kind::X86_STMXCSR: return VisitX86_StmXCSR(static_cast<X86_StmXCSRInst *>(i));
    case Inst::Kind::X86_FNCLEX:  return VisitX86_FnClEx(static_cast<X86_FnClExInst *>(i));
    case Inst::Kind::AARCH64_LL:  return VisitAArch64_LL(static_cast<AArch64_LL *>(i));
    case Inst::Kind::AARCH64_SC:  return VisitAArch64_SC(static_cast<AArch64_SC *>(i));
    case Inst::Kind::AARCH64_DMB: return VisitAArch64_DMB(static_cast<AArch64_DMB *>(i));
  }
  llvm_unreachable("invalid instruction kind");
}
