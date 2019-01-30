// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts.h"
#include "core/symbol.h"



// -----------------------------------------------------------------------------
SelectInst::SelectInst(Type type, Inst *cond, Inst *vt, Inst *vf)
  : OperatorInst(Kind::SELECT, type, 3)
{
  Op<0>() = cond;
  Op<1>() = vt;
  Op<2>() = vf;
}

// -----------------------------------------------------------------------------
SetInst::SetInst(ConstantReg *reg, Inst *val)
  : Inst(Kind::SET, 2)
{
  Op<0>() = reg;
  Op<1>() = val;
}

// -----------------------------------------------------------------------------
unsigned SetInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type SetInst::GetType(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
ArgInst::ArgInst(Type type, ConstantInt *index)
  : ConstInst(Kind::ARG, type, 1)
{
  Op<0>() = index;
}

// -----------------------------------------------------------------------------
unsigned ArgInst::GetIdx() const
{
  return static_cast<ConstantInt *>(Op<0>().get())->GetValue();
}

// -----------------------------------------------------------------------------
FrameInst::FrameInst(Type type, ConstantInt *index)
  : ConstInst(Kind::FRAME, type, 1)
{
  Op<0>() = index;
}

// -----------------------------------------------------------------------------
unsigned FrameInst::GetIdx() const
{
  return static_cast<ConstantInt *>(Op<0>().get())->GetValue();
}

// -----------------------------------------------------------------------------
VAStartInst::VAStartInst(Inst *vaList)
  : Inst(Kind::VASTART, 1)
{
  Op<0>() = vaList;
}

// -----------------------------------------------------------------------------
unsigned VAStartInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type VAStartInst::GetType(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
UndefInst::UndefInst(Type type)
  : ConstInst(Kind::UNDEF, type, 0)
{
}

// -----------------------------------------------------------------------------
PhiInst::PhiInst(Type type)
  : Inst(Kind::PHI, 0)
  , type_(type)
{
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type PhiInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void PhiInst::Add(Block *block, Value *value)
{
  for (unsigned i = 0, n = GetNumIncoming(); i < n; ++i) {
    if (GetBlock(i) == block) {
      *(op_begin() + i * 2 + 1) = value;
      return;
    }
  }
  growUses(numOps_ + 2);
  Op<-2>() = block;
  Op<-1>() = value;
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumIncoming() const
{
  assert((numOps_ & 1) == 0 && "invalid node count");
  return numOps_ >> 1;
}

// -----------------------------------------------------------------------------
Block *PhiInst::GetBlock(unsigned i) const
{
  return static_cast<Block *>((op_begin() + i * 2 + 0)->get());
}

// -----------------------------------------------------------------------------
void PhiInst::SetBlock(unsigned i, Block *block)
{
  *(op_begin() + i * 2) = block;
}

// -----------------------------------------------------------------------------
Value *PhiInst::GetValue(unsigned i) const
{
  return static_cast<Block *>((op_begin() + i * 2 + 1)->get());
}

// -----------------------------------------------------------------------------
bool PhiInst::HasValue(const Block *block) const
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
Value *PhiInst::GetValue(const Block *block) const
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return GetValue(i);
    }
  }
  throw InvalidPredecessorException();
}
