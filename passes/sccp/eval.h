// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/insts.h"
#include "passes/sccp/lattice.h"



/**
 * Class implementing SCCP evaluation.
 */
class SCCPEval {
public:
  /// Performs type conversions, if necessary.
  static Lattice Extend(const Lattice &arg, Type ty);
  /// Evaluates a unary instruction.
  static Lattice Eval(UnaryInst *inst, Lattice &arg);
  /// Evaluates a binary instruction.
  static Lattice Eval(BinaryInst *inst, Lattice &lhs, Lattice &rhs);
  /// Evaluates a load from constant data.
  static Lattice Eval(LoadInst *inst, Lattice &addr);

private:
  static Lattice Eval(AbsInst *inst, Lattice &arg);
  static Lattice Eval(NegInst *inst, Lattice &arg);
  static Lattice Eval(SqrtInst *inst, Lattice &arg);
  static Lattice Eval(SinInst *inst, Lattice &arg);
  static Lattice Eval(CosInst *inst, Lattice &arg);
  static Lattice Eval(SExtInst *inst, Lattice &arg);
  static Lattice Eval(ZExtInst *inst, Lattice &arg);
  static Lattice Eval(XExtInst *inst, Lattice &arg);
  static Lattice Eval(FExtInst *inst, Lattice &arg);
  static Lattice Eval(TruncInst *inst, Lattice &arg);
  static Lattice Eval(ExpInst *inst, Lattice &arg);
  static Lattice Eval(Exp2Inst *inst, Lattice &arg);
  static Lattice Eval(LogInst *inst, Lattice &arg);
  static Lattice Eval(Log2Inst *inst, Lattice &arg);
  static Lattice Eval(Log10Inst *inst, Lattice &arg);
  static Lattice Eval(FCeilInst *inst, Lattice &arg);
  static Lattice Eval(FFloorInst *inst, Lattice &arg);
  static Lattice Eval(PopCountInst *inst, Lattice &arg);
  static Lattice Eval(CtzInst *inst, Lattice &arg);
  static Lattice Eval(ClzInst *inst, Lattice &arg);
  static Lattice Eval(ByteSwapInst *inst, Lattice &arg);
  static Lattice Eval(BitCastInst *inst, Lattice &arg);

  static Lattice Eval(AddInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(AndInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(UDivInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SDivInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(URemInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SRemInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(MulInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(MulHSInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(MulHUInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(OrInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SubInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(XorInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(PowInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(CopySignInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(OUAddInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(OUMulInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(OUSubInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(OSAddInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(OSMulInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(OSSubInst *inst, Lattice &lhs, Lattice &rhs);

  /// Enumeration of shift operations.
  enum class Bitwise {
    SRL,
    SRA,
    SLL,
    ROTL,
    ROTR
  };

  /// Evaluates a bitwise operator.
  static Lattice Eval(Bitwise kind, Type ty, Lattice &lhs, Lattice &rhs);
};
