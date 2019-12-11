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
  llvm_unreachable("invalid operand");
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
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
UnaryInst::UnaryInst(
    Kind kind,
    Type type,
    Inst *arg,
    const AnnotSet &annot)
  : OperatorInst(kind, type, 1, annot)
{
  Op<0>() = arg;
}

// -----------------------------------------------------------------------------
Inst *UnaryInst::GetArg() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
BinaryInst::BinaryInst(
    Kind kind,
    Type type,
    Inst *lhs,
    Inst *rhs,
    const AnnotSet &annot)
  : OperatorInst(kind, type, 2, annot)
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

