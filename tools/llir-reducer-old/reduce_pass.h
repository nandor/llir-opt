// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <random>

#include "core/pass.h"
#include "core/insts.h"

class Func;



/**
 * Pass to eliminate unnecessary moves.
 */
class ReducePass final : public Pass {
public:
  /// Initialises the pass.
  ReducePass(PassManager *passManager, unsigned seed)
    : Pass(passManager), rand_(seed)
  {}

  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Reduces a program.
  void Reduce(Prog *p);

  /// Reduces a functio.
  void ReduceInst(Inst *f);

  /// Reduces an argument instruction.
  void ReduceArg(ArgInst *i);
  /// Reduces a frame instruction.
  void ReduceFrame(FrameInst *i);
  /// Reduces a call.
  void ReduceCall(CallInst *i);
  /// Reduces an invoke instruction.
  void ReduceInvoke(InvokeInst *i);
  /// Reduces a tail call instruction.
  void ReduceTailCall(TailCallInst *i);
  /// Reduces a load instruction.
  void ReduceLoad(LoadInst *i);
  /// Reduces a store instruction.
  void ReduceStore(StoreInst *i);
  /// Reduces a mov instruction.
  void ReduceMov(MovInst *i);
  /// Reduces a unary instruction.
  void ReduceUnary(UnaryInst *i);
  /// Reduces a binary instruction.
  void ReduceBinary(BinaryInst *i);
  /// Reduces a switch instruction.
  void ReduceSwitch(SwitchInst *i);
  /// Reduces a jmp instruction.
  void ReduceJmp(JumpInst *i);
  /// Reduces a jcc instruction.
  void ReduceJcc(JumpCondInst *i);
  /// Reduces a ret instruction.
  void ReduceRet(ReturnInst *i);
  /// Reduces a phi instruction.
  void ReducePhi(PhiInst *phi);
  /// Reduces a select instruction.
  void ReduceSelect(SelectInst *select);
  /// Reduces a FNSTCW instruction.
  void ReduceFNStCw(FNStCwInst *i);
  /// Reduces a FLDCW instruction.
  void ReduceFLdCw(FLdCwInst *i);

  /// Reduces a value to undefined.
  void ReduceUndefined(Inst *i);
  /// Reduces a value to zero.
  void ReduceZero(Inst *i);
  /// Reduces a value by erasing it.
  void ReduceErase(Inst *i);
  /// Removes a flow edge.
  void RemoveEdge(Block *from, Block *to);
  /// Removes an argument from a call.
  template <typename T>
  void RemoveArg(T *i);
  /// Reduce an instruction to one of its arguments.
  void ReduceOp(Inst *i, Inst *op);
  /// Reduce to an argument, if one of the correct type is available.
  void ReduceToArg(Inst *i);

  /// Returns a zero value of any type.
  Constant *GetZero(Type type);

  /// Returns a random number in a range.
  unsigned Random(unsigned n);

private:
  /// Random generator.
  std::mt19937 rand_;
};

