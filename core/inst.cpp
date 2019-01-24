// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/inst.h"
#include "core/insts.h"



// -----------------------------------------------------------------------------
Inst::~Inst()
{
}

// -----------------------------------------------------------------------------
void Inst::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Inst::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
unsigned TerminatorInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type TerminatorInst::GetType(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned OperatorInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type OperatorInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
UnaryInst::UnaryInst(Kind kind, Type type, Inst *arg)
  : OperatorInst(kind, type, 1)
{
  Op<0>() = arg;
}

// -----------------------------------------------------------------------------
Inst *UnaryInst::GetArg() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
BinaryInst::BinaryInst(Kind kind, Type type, Inst *lhs, Inst *rhs)
  : OperatorInst(kind, type, 2)
{
  Op<0>() = lhs;
  Op<1>() = rhs;
}

// -----------------------------------------------------------------------------
Inst *BinaryInst::GetLHS() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
Inst *BinaryInst::GetRHS() const
{
  return static_cast<Inst *>(Op<1>().get());
}

