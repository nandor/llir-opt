// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/insts.h"



class InstVisitor {
public:
  virtual ~InstVisitor();

  void Dispatch(Inst *i);

public:
  virtual void Visit(Inst *i) = 0;
  virtual void VisitUnary(UnaryInst *i) { return Visit(i); }
  virtual void VisitBinary(BinaryInst *i) { return Visit(i); }

public:
  virtual void VisitCall(CallInst *i) { return Visit(i); }
  virtual void VisitTailCall(TailCallInst *i) { return Visit(i); }
  virtual void VisitInvoke(InvokeInst *i) { return Visit(i); }
  virtual void VisitTailInvoke(TailInvokeInst *i) { return Visit(i); }
  virtual void VisitReturn(ReturnInst *i) { return Visit(i); }
  virtual void VisitJumpCond(JumpCondInst *i) { return Visit(i); }
  virtual void VisitJumpIndirect(JumpIndirectInst *i) { return Visit(i); }
  virtual void VisitJump(JumpInst *i) { return Visit(i); }
  virtual void VisitSwitch(SwitchInst *i) { return Visit(i); }
  virtual void VisitTrap(TrapInst *i) { return Visit(i); }
  virtual void VisitSyscall(SyscallInst *i) { return Visit(i); }
  virtual void VisitSet(SetInst *i) { return Visit(i); }
  virtual void VisitMov(MovInst *i) { return Visit(i); }
  virtual void VisitLoad(LoadInst *i) { return Visit(i); }
  virtual void VisitStore(StoreInst *i) { return Visit(i); }
  virtual void VisitXchg(XchgInst *i) { return Visit(i); }
  virtual void VisitCmpXchg(CmpXchgInst *i) { return Visit(i); }
  virtual void VisitVAStart(VAStartInst *i) { return Visit(i); }
  virtual void VisitAlloca(AllocaInst *i) { return Visit(i); }
  virtual void VisitArg(ArgInst *i) { return Visit(i); }
  virtual void VisitFrame(FrameInst *i) { return Visit(i); }
  virtual void VisitUndef(UndefInst *i) { return Visit(i); }
  virtual void VisitRdtsc(RdtscInst *i) { return Visit(i); }
  virtual void VisitFNStCw(FNStCwInst *i) { return Visit(i); }
  virtual void VisitFLdCw(FLdCwInst *i) { return Visit(i); }
  virtual void VisitSelect(SelectInst *i) { return Visit(i); }
  virtual void VisitAbs(AbsInst *i) { return VisitUnary(i); }
  virtual void VisitNeg(NegInst *i) { return VisitUnary(i); }
  virtual void VisitSqrt(SqrtInst *i) { return VisitUnary(i); }
  virtual void VisitSin(SinInst *i) { return VisitUnary(i); }
  virtual void VisitCos(CosInst *i) { return VisitUnary(i); }
  virtual void VisitSExt(SExtInst *i) { return VisitUnary(i); }
  virtual void VisitZExt(ZExtInst *i) { return VisitUnary(i); }
  virtual void VisitFExt(FExtInst *i) { return VisitUnary(i); }
  virtual void VisitXExt(XExtInst *i) { return VisitUnary(i); }
  virtual void VisitTrunc(TruncInst *i) { return VisitUnary(i); }
  virtual void VisitExp(ExpInst *i) { return VisitUnary(i); }
  virtual void VisitExp2(Exp2Inst *i) { return VisitUnary(i); }
  virtual void VisitLog(LogInst *i) { return VisitUnary(i); }
  virtual void VisitLog2(Log2Inst *i) { return VisitUnary(i); }
  virtual void VisitLog10(Log10Inst *i) { return VisitUnary(i); }
  virtual void VisitFCeil(FCeilInst *i) { return VisitUnary(i); }
  virtual void VisitFFloor(FFloorInst *i) { return VisitUnary(i); }
  virtual void VisitPopCount(PopCountInst *i) { return VisitUnary(i); }
  virtual void VisitCLZ(CLZInst *i) { return VisitUnary(i); }
  virtual void VisitCTZ(CTZInst *i) { return VisitUnary(i); }
  virtual void VisitAdd(AddInst *i) { return VisitBinary(i); }
  virtual void VisitAnd(AndInst *i) { return VisitBinary(i); }
  virtual void VisitCmp(CmpInst *i) { return VisitBinary(i); }
  virtual void VisitUDiv(UDivInst *i) { return VisitBinary(i); }
  virtual void VisitURem(URemInst *i) { return VisitBinary(i); }
  virtual void VisitSDiv(SDivInst *i) { return VisitBinary(i); }
  virtual void VisitSRem(SRemInst *i) { return VisitBinary(i); }
  virtual void VisitMul(MulInst *i) { return VisitBinary(i); }
  virtual void VisitOr(OrInst *i) { return VisitBinary(i); }
  virtual void VisitRotl(RotlInst *i) { return VisitBinary(i); }
  virtual void VisitRotr(RotrInst *i) { return VisitBinary(i); }
  virtual void VisitSll(SllInst *i) { return VisitBinary(i); }
  virtual void VisitSra(SraInst *i) { return VisitBinary(i); }
  virtual void VisitSrl(SrlInst *i) { return VisitBinary(i); }
  virtual void VisitSub(SubInst *i) { return VisitBinary(i); }
  virtual void VisitXor(XorInst *i) { return VisitBinary(i); }
  virtual void VisitPow(PowInst *i) { return VisitBinary(i); }
  virtual void VisitCopySign(CopySignInst *i) { return VisitBinary(i); }
  virtual void VisitAddUO(AddUOInst *i) { return VisitBinary(i); }
  virtual void VisitMulUO(MulUOInst *i) { return VisitBinary(i); }
  virtual void VisitSubUO(SubUOInst *i) { return VisitBinary(i); }
  virtual void VisitAddSO(AddSOInst *i) { return VisitBinary(i); }
  virtual void VisitMulSO(MulSOInst *i) { return VisitBinary(i); }
  virtual void VisitSubSO(SubSOInst *i) { return VisitBinary(i); }
  virtual void VisitPhi(PhiInst *i) { return Visit(i); }
};
