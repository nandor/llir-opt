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
unsigned TerminatorInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
unsigned UnaryOperatorInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned UnaryOperatorInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &UnaryOperatorInst::GetOp(unsigned i) const
{
  if (i == 0) return arg_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void UnaryOperatorInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { arg_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned BinaryOperatorInst::GetNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
unsigned BinaryOperatorInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &BinaryOperatorInst::GetOp(unsigned i) const
{
  if (i == 0) return lhs_;
  if (i == 1) return rhs_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void BinaryOperatorInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { lhs_ = op; return; }
  if (i == 1) { rhs_ = op; return; }
  throw InvalidOperandException();
}
