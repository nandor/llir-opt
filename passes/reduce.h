// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <random>

#include "core/pass.h"

class Func;
class ArgInst;
class CallInst;
class LoadInst;
class StoreInst;
class MovInst;
class SwitchInst;
class JumpInst;
class JumpCondInst;
class ReturnInst;
class PhiInst;



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
  /// Reduces an argument instruction.
  void ReduceArg(ArgInst *i);
  /// Reduces a call.
  void ReduceCall(CallInst *i);
  /// Reduces a load instruction.
  void ReduceLoad(LoadInst *i);
  /// Reduces a store instruction.
  void ReduceStore(StoreInst *i);
  /// Reduces a mov instruction.
  void ReduceMov(MovInst *i);
  /// Reduces a binary instruction.
  void ReduceBinary(Inst *i);
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

  /// Reduces a value to undefined.
  void ReduceUndefined(Inst *i);
  /// Reduces a value by erasing it.
  void ReduceErase(Inst *i);
  /// Removes a flow edge.
  void RemoveEdge(Block *from, Block *to);

  /// Returns a random number in a range.
  unsigned Random(unsigned n);

private:
  /// Random generator.
  std::mt19937 rand_;
};