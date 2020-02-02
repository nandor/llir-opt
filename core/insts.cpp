// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts.h"
#include "core/symbol.h"



// -----------------------------------------------------------------------------
SelectInst::SelectInst(
    Type type,
    Inst *cond,
    Inst *vt,
    Inst *vf,
    const AnnotSet &annot)
  : OperatorInst(Kind::SELECT, type, 3, annot)
{
  Op<0>() = cond;
  Op<1>() = vt;
  Op<2>() = vf;
}

// -----------------------------------------------------------------------------
SetInst::SetInst(ConstantReg *reg, Inst *val, const AnnotSet &annot)
  : Inst(Kind::SET, 2, annot)
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
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
VAStartInst::VAStartInst(Inst *vaList, const AnnotSet &annot)
  : Inst(Kind::VASTART, 1, annot)
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
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
AllocaInst::AllocaInst(
    Type type,
    Inst *size,
    ConstantInt *align,
    const AnnotSet &annot)
  : OperatorInst(Kind::ALLOCA, type, 2, annot)
{
  Op<0>() = size;
  Op<1>() = align;
}

// -----------------------------------------------------------------------------
PhiInst::PhiInst(Type type, const AnnotSet &annot)
  : Inst(Kind::PHI, 0, annot)
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
  llvm_unreachable("invalid operand");
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
  resizeUses(numOps_ + 2);
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
  *(op_begin() + i * 2 + 0) = block;
}

// -----------------------------------------------------------------------------
void PhiInst::SetValue(unsigned i, Value *value)
{
  *(op_begin() + i * 2 + 1) = value;
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
  llvm_unreachable("invalid predecessor");
}

// -----------------------------------------------------------------------------
void PhiInst::Remove(const Block *block) {
  for (unsigned i = 0, n = GetNumIncoming(); i < n; ++i) {
    if (GetBlock(i) == block) {
      if (i != n - 1) {
        *(op_begin() + i * 2 + 0) = GetBlock(n - 1);
        *(op_begin() + i * 2 + 1) = GetValue(n - 1);
      }
      resizeUses(numOps_ - 2);
      return;
    }
  }
  llvm_unreachable("invalid predecessor");
}
