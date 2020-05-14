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
  /// Performs a bitcast from one type to another.
  static Lattice Bitcast(const Lattice &arg, Type ty);

private:
  static Lattice Eval(AbsInst *inst, Lattice &arg);
  static Lattice Eval(NegInst *inst, Lattice &arg);
  static Lattice Eval(SqrtInst *inst, Lattice &arg);
  static Lattice Eval(SinInst *inst, Lattice &arg);
  static Lattice Eval(CosInst *inst, Lattice &arg);
  static Lattice Eval(SExtInst *inst, Lattice &arg);
  static Lattice Eval(ZExtInst *inst, Lattice &arg);
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
  static Lattice Eval(CLZInst *inst, Lattice &arg);

  static Lattice Eval(AddInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(AndInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(CmpInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(UDivInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SDivInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(URemInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SRemInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(MulInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(OrInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(RotlInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(RotrInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SllInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SraInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SrlInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SubInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(XorInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(PowInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(CopySignInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(AddUOInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(MulUOInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SubUOInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(AddSOInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(MulSOInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SubSOInst *inst, Lattice &lhs, Lattice &rhs);

  /// Enumeration of shift operations.
  enum class Bitwise {
    SRL,
    SRA,
    SLL,
    ROTL
  };

  /// Evaluates a bitwise operator.
  static Lattice Eval(Bitwise kind, Type ty, Lattice &lhs, Lattice &rhs);
};
