// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <random>
#include <queue>

#include "core/insts.h"
#include "timeout.h"

class Func;



/**
 * Pass to eliminate unnecessary moves.
 */
class InstReducerBase {
public:
  /// Initialises the pass.
  InstReducerBase(unsigned threads) : threads_(threads) {}

  /// Runs the pass.
  std::unique_ptr<Prog> Reduce(
      std::unique_ptr<Prog> &&prog,
      const Timeout &Timeout
  );

  /// Verifies a program.
  virtual bool Verify(const Prog &prog) const = 0;

private:
  using It = std::optional<std::pair<std::unique_ptr<Prog>, Inst *>>;
  using Bt = std::optional<std::pair<std::unique_ptr<Prog>, Block *>>;
  using At = std::optional<std::pair<std::unique_ptr<Prog>, Atom *>>;
  using Ft = std::optional<std::pair<std::unique_ptr<Prog>, Func *>>;

  /// Reduces an instruction in a function.
  It ReduceInst(Inst *i);
  /// Reduces a block.
  Bt ReduceBlock(Block *b);
  /// Reduces an atom.
  At ReduceAtom(Atom *a);
  /// Reduces a function.
  Ft ReduceFunc(Func *f);

  /// Reduces an argument instruction.
  It ReduceArg(ArgInst *i);
  /// Reduces a call.
  It ReduceCall(CallInst *i);
  /// Reduces an invoke instruction.
  It ReduceInvoke(InvokeInst *i);
  /// Reduces a tail call instruction.
  It ReduceTailCall(TailCallInst *i);
  /// Reduces a system call instruction.
  It ReduceSyscall(SyscallInst *i);
  /// Reduces a process clone instruction.
  It ReduceClone(CloneInst *i);
  /// Reduces a jump indirect instruction.
  It ReduceRaise(RaiseInst *i);
  /// Reduces a store instruction.
  It ReduceStore(StoreInst *i);
  /// Reduces a mov instruction.
  It ReduceMov(MovInst *i);
  /// Reduces a switch instruction.
  It ReduceSwitch(SwitchInst *i);
  /// Reduces a jmp instruction.
  It ReduceJmp(JumpInst *i);
  /// Reduces a jcc instruction.
  It ReduceJcc(JumpCondInst *i);
  /// Reduces a ret instruction.
  It ReduceReturn(ReturnInst *i);
  /// Reduces a retjmp instruction.
  It ReduceReturnJump(ReturnJumpInst *i);
  /// Reduces a phi instruction.
  It ReducePhi(PhiInst *phi);
  /// Reduces a FNSTCW instruction.
  It ReduceFNStCw(FNStCwInst *i);
  /// Reduces a undefined instruction.
  It ReduceUndef(UndefInst *i);
  /// Reduces a vastart instruction.
  It ReduceVAStart(VAStartInst *i);
  /// Reduces a set instruction.
  It ReduceSet(SetInst *i);
  /// Reduces an xchg instruction.
  It ReduceXchg(XchgInst *i) { return ReduceOperator(i); }
  /// Reduces a cmpxchg instruction.
  It ReduceCmpXchg(CmpXchgInst *i) { return ReduceOperator(i); }
  /// Reduces an alloca instruction.
  It ReduceAlloca(AllocaInst *i) { return ReduceOperator(i); }
  /// Reduces a frame instruction.
  It ReduceFrame(FrameInst *i) { return ReduceOperator(i); }
  /// Reduces a load instruction.
  It ReduceLoad(LoadInst *i) { return ReduceOperator(i); }
  /// Reduces a unary instruction.
  It ReduceUnary(UnaryInst *i) { return ReduceOperator(i); }
  /// Reduces a binary instruction.
  It ReduceBinary(BinaryInst *i) { return ReduceOperator(i); }
  /// Reduces a select instruction.
  It ReduceSelect(SelectInst *i) { return ReduceOperator(i); }
  /// Reduces a FLDCW instruction.
  It ReduceFLdCw(FLdCwInst *i) { return ReduceOperator(i); }
  /// Reduces a RDTSC instruction.
  It ReduceRdtsc(RdtscInst *i) { return ReduceOperator(i); }

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
