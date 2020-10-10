// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <random>
#include <queue>

#include "core/inst_visitor.h"
#include "core/insts.h"
#include "timeout.h"

class Func;

template <typename T>
using Iterator = std::optional<std::pair<std::unique_ptr<Prog>, T *>>;



/**
 * Pass to eliminate unnecessary moves.
 */
class ProgReducerBase : public InstVisitor<Iterator<Inst>> {
public:
  /// Initialises the pass.
  ProgReducerBase(unsigned threads) : threads_(threads) {}

  /// Runs the pass.
  std::unique_ptr<Prog> Reduce(
      std::unique_ptr<Prog> &&prog,
      const Timeout &Timeout
  );

  /// Verifies a program.
  virtual bool Verify(const Prog &prog) const = 0;

private:
  using It = Iterator<Inst>;
  using Bt = Iterator<Block>;
  using At = Iterator<Atom>;
  using Ft = Iterator<Func>;

  /// Reduces an instruction in a function.
  It ReduceInst(Inst *i) { return Dispatch(i); }
  /// Reduces a block.
  Bt ReduceBlock(Block *b);
  /// Reduces an atom.
  At ReduceAtom(Atom *a);
  /// Reduces a function.
  Ft ReduceFunc(Func *f);

  /// Reduces an argument instruction.
  It VisitArg(ArgInst *i);
  /// Reduces a call.
  It VisitCall(CallInst *i);
  /// Reduces an invoke instruction.
  It VisitInvoke(InvokeInst *i);
  /// Reduces a tail call instruction.
  It VisitTailCall(TailCallInst *i);
  /// Reduces a system call instruction.
  It VisitSyscall(SyscallInst *i);
  /// Reduces a process clone instruction.
  It VisitClone(CloneInst *i);
  /// Reduces a jump indirect instruction.
  It VisitRaise(RaiseInst *i);
  /// Reduces a store instruction.
  It VisitStore(StoreInst *i);
  /// Reduces a mov instruction.
  It VisitMov(MovInst *i);
  /// Reduces a switch instruction.
  It VisitSwitch(SwitchInst *i);
  /// Reduces a jmp instruction.
  It VisitJmp(JumpInst *i);
  /// Reduces a jcc instruction.
  It VisitJcc(JumpCondInst *i);
  /// Reduces a ret instruction.
  It VisitReturn(ReturnInst *i);
  /// Reduces a retjmp instruction.
  It VisitReturnJump(ReturnJumpInst *i);
  /// Reduces a phi instruction.
  It VisitPhi(PhiInst *phi);
  /// Reduces a undefined instruction.
  It VisitUndef(UndefInst *i);
  /// Reduces a vastart instruction.
  It VisitVAStart(VAStartInst *i);
  /// Reduces a set instruction.
  It VisitSet(SetInst *i);
  /// Reduces an alloca instruction.
  It VisitAlloca(AllocaInst *i) { return ReduceOperator(i); }
  /// Reduces a frame instruction.
  It VisitFrame(FrameInst *i) { return ReduceOperator(i); }
  /// Reduces a load instruction.
  It VisitLoad(LoadInst *i) { return ReduceOperator(i); }
  /// Reduces a unary instruction.
  It VisitUnary(UnaryInst *i) { return ReduceOperator(i); }
  /// Reduces a binary instruction.
  It VisitBinary(BinaryInst *i) { return ReduceOperator(i); }
  /// Reduces a select instruction.
  It VisitSelect(SelectInst *i) { return ReduceOperator(i); }
  /// Reduces an xchg instruction.
  It VisitXchg(X86_XchgInst *i) { return ReduceOperator(i); }
  /// Reduces a cmpxchg instruction.
  It VisitCmpXchg(X86_CmpXchgInst *i) { return ReduceOperator(i); }
  /// Reduces a RDTSC instruction.
  It VisitRdtsc(X86_RdtscInst *i) { return ReduceOperator(i); }
  /// Reduces a FPU control instruction.
  It VisitX86_FPUControlInst(X86_FPUControlInst *i);
  /// Reduces a generic instruction.
  It Visit(Inst *i);

  /// Generic value reduction.
  It ReduceOperator(Inst *i);

  using Candidate = std::pair<std::unique_ptr<Prog>, Inst *>;
  using CandidateList = std::queue<Candidate>;

  /// Removes an argument from a call.
  template <typename T>
  void RemoveArg(CandidateList &cand, T *i);
  /// Reduce an instruction to one of its arguments.
  void ReduceToOp(CandidateList &cand, Inst *i);
  /// Reduce an instruction to a return of one of the arguments.
  void ReduceToRet(CandidateList &cand, Inst *i);
  /// Reduces a value to trap.
  void ReduceToTrap(CandidateList &cand, Inst *i);
  /// Reduces a value to undefined.
  void ReduceToUndef(CandidateList &cand, Inst *i);
  /// Reduces a value to zero.
  void ReduceZero(CandidateList &cand, Inst *i);
  /// Reduces a value by erasing it.
  void ReduceErase(CandidateList &cand, Inst *i);
  /// Reduce to an argument, if one of the correct type is available.
  void ReduceToArg(CandidateList &cand, Inst *i);
  /// Generic value reduction.
  void ReduceOperator(CandidateList &cand, Inst *i);

  /// Evaluate multiple candidates in parallel.
  It Evaluate(CandidateList &&cand);

  /// Removes a flow edge.
  void RemoveEdge(Block *from, Block *to);

  /// Returns a zero value of any type.
  Constant *GetZero(Type type);

private:
  /// Number of threads to use.
  unsigned threads_;
};
