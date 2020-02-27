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
  /// Evaluates a unary instruction.
  static Lattice Eval(UnaryInst *inst, Lattice &arg);
  /// Evaluates a binary instruction.
  static Lattice Eval(BinaryInst *inst, Lattice &lhs, Lattice &rhs);

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

  static Lattice Eval(AddInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(SubInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(AndInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(OrInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(XorInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(PowInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(CopySignInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(AddUOInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(MulUOInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(CmpInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(DivInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(RemInst *inst, Lattice &lhs, Lattice &rhs);
  static Lattice Eval(MulInst *inst, Lattice &lhs, Lattice &rhs);

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
