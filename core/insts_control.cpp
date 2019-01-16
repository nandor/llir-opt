// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts_control.h"



// -----------------------------------------------------------------------------
ReturnInst::ReturnInst()
  : TerminatorInst(Kind::RET, 0)
{
}

// -----------------------------------------------------------------------------
ReturnInst::ReturnInst(Type t, Inst *op)
  : TerminatorInst(Kind::RET, 1)
{
  Op<0>() = op;
}

// -----------------------------------------------------------------------------
Block *ReturnInst::getSuccessor(unsigned i) const
{
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned ReturnInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Inst *ReturnInst::GetValue() const
{
  return numOps_ > 0 ? static_cast<Inst *>(Op<0>().get()) : nullptr;
}

// -----------------------------------------------------------------------------
JumpCondInst::JumpCondInst(Value *cond, Block *bt, Block *bf)
  : TerminatorInst(Kind::JCC, 3)
{
  Op<0>() = cond;
  Op<1>() = bt;
  Op<2>() = bf;
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::getSuccessor(unsigned i) const
{
  if (i == 0) return GetTrueTarget();
  if (i == 1) return GetFalseTarget();
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned JumpCondInst::getNumSuccessors() const
{
  return 2;
}

// -----------------------------------------------------------------------------
Inst *JumpCondInst::GetCond() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::GetTrueTarget() const
{
  return static_cast<Block *>(Op<1>().get());
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::GetFalseTarget() const
{
  return static_cast<Block *>(Op<2>().get());
}

// -----------------------------------------------------------------------------
JumpIndirectInst::JumpIndirectInst(Inst *target)
  : TerminatorInst(Kind::JI, 1)
{
  Op<0>() = target;
}

// -----------------------------------------------------------------------------
Block *JumpIndirectInst::getSuccessor(unsigned i) const
{
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned JumpIndirectInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
JumpInst::JumpInst(Block *target)
  : TerminatorInst(Kind::JMP, 1)
{
  Op<0>() = target;
}

// -----------------------------------------------------------------------------
Block *JumpInst::getSuccessor(unsigned i) const
{
  if (i == 0) return static_cast<Block *>(Op<0>().get());
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned JumpInst::getNumSuccessors() const
{
  return 1;
}

// -----------------------------------------------------------------------------
SwitchInst::SwitchInst(Inst *index, const std::vector<Value *> &branches)
  : TerminatorInst(Kind::SWITCH, branches.size() + 1)
{
  Op<0>() = index;
  for (unsigned i = 0, n = branches.size(); i < n; ++i) {
    *(op_begin() + i + 1) = branches[i];
  }
}

// -----------------------------------------------------------------------------
Block *SwitchInst::getSuccessor(unsigned i) const
{
  if (i + 1 < numOps_) return static_cast<Block *>((op_begin() + i + 1)->get());
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned SwitchInst::getNumSuccessors() const
{
  return numOps_ - 1;
}

// -----------------------------------------------------------------------------
Block *TrapInst::getSuccessor(unsigned i) const
{
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned TrapInst::getNumSuccessors() const
{
  return 0;
}
