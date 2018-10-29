// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/inst.h"
#include "core/insts.h"



// -----------------------------------------------------------------------------
Expr *Expr::CreateSymbolOff(Context &ctx, Symbol *sym, int64_t offset)
{
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst::~Inst()
{
}

// -----------------------------------------------------------------------------
unsigned UnaryOperatorInst::getNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &UnaryOperatorInst::getOp(unsigned i) const
{
  if (i == 0) return arg_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned BinaryOperatorInst::getNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
const Operand &BinaryOperatorInst::getOp(unsigned i) const
{
  if (i == 0) return lhs_;
  if (i == 1) return rhs_;
  throw InvalidOperandException();
}
