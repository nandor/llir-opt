// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <random>

#include "core/insts.h"
#include "timeout.h"

class Func;



/**
 * Pass to eliminate unnecessary moves.
 */
class InstReducerBase {
public:
  /// Initialises the pass.
  InstReducerBase() {}

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

  /// Removes an argument from a call.
  template <typename T>
  It RemoveArg(Prog &p, T *i);

  /// Tries a reduction rule.
  template <typename... Args>
  It TryReducer(
      Inst *(InstReducerBase::*f)(Inst *, Args...),
      Prog &p,
      Inst *i,
      Args...
  );

  /// Reduces an instruction in a function.
  It ReduceInst(Prog &p, Inst *i);
  /// Reduces a block.
  Bt ReduceBlock(Prog &p, Block *b);

  /// Reduces an argument instruction.
  It ReduceArg(Prog &p, ArgInst *i);
  /// Reduces a frame instruction.
  It ReduceFrame(Prog &p, FrameInst *i) { return ReduceOperator(p, i); }
  /// Reduces a call.
  It ReduceCall(Prog &p, CallInst *i);
  /// Reduces an invoke instruction.
  It ReduceInvoke(Prog &p, InvokeInst *i);
  /// Reduces a tail call instruction.
  It ReduceTailCall(Prog &p, TailCallInst *i);
  /// Reduces a load instruction.
  It ReduceLoad(Prog &p, LoadInst *i) { return ReduceOperator(p, i); }
  /// Reduces a store instruction.
  It ReduceStore(Prog &p, StoreInst *i);
  /// Reduces a mov instruction.
  It ReduceMov(Prog &p, MovInst *i);
  /// Reduces a unary instruction.
  It ReduceUnary(Prog &p, UnaryInst *i) { return ReduceOperator(p, i); }
  /// Reduces a binary instruction.
  It ReduceBinary(Prog &p, BinaryInst *i) { return ReduceOperator(p, i); }
  /// Reduces a switch instruction.
  It ReduceSwitch(Prog &p, SwitchInst *i);
  /// Reduces a jmp instruction.
  It ReduceJmp(Prog &p, JumpInst *i);
  /// Reduces a jcc instruction.
  It ReduceJcc(Prog &p, JumpCondInst *i);
  /// Reduces a ret instruction.
  It ReduceRet(Prog &p, ReturnInst *i);
  /// Reduces a phi instruction.
  It ReducePhi(Prog &p, PhiInst *phi);
  /// Reduces a select instruction.
  It ReduceSelect(Prog &p, SelectInst *i) { return ReduceOperator(p, i); }
  /// Reduces a FNSTCW instruction.
  It ReduceFNStCw(Prog &p, FNStCwInst *i);
  /// Reduces a FLDCW instruction.
  It ReduceFLdCw(Prog &p, FLdCwInst *i) { return ReduceOperator(p, i); }
  /// Reduces a RDTSC instruction.
  It ReduceRdtsc(Prog &p, RdtscInst *i) { return ReduceOperator(p, i); }
  /// Reduces a undefined instruction.
  It ReduceUndef(Prog &p, UndefInst *i);

  /// Generic value reduction.
  It ReduceOperator(Prog &p, Inst *i);

  /// Reduce an instruction to one of its arguments.
  It ReduceToOp(Prog &p, Inst *i);
  /// Reduce an instruction to a return of one of the arguments.
  It ReduceToRet(Prog &p, Inst *i);
  /// Reduces a value to trap.
  Inst *ReduceTrap(Inst *i);
  /// Reduces a value to undefined.
  Inst *ReduceToUndef(Inst *i);
  /// Reduces a value to zero.
  Inst *ReduceZero(Inst *i);
  /// Reduces a value by erasing it.
  Inst *ReduceErase(Inst *i);
  /// Reduce to an argument, if one of the correct type is available.
  Inst *ReduceToArg(Inst *i);

  /// Removes a flow edge.
  void RemoveEdge(Block *from, Block *to);

  /// Returns a zero value of any type.
  Constant *GetZero(Type type);
};
