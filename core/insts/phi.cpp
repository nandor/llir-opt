// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts.h"



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
void PhiInst::Add(Block *block, Inst *inst)
{
  for (unsigned i = 0, n = GetNumIncoming(); i < n; ++i) {
    if (GetBlock(i) == block) {
      *(op_begin() + i * 2 + 1) = inst;
      return;
    }
  }
  resizeUses(numOps_ + 2);
  Op<-2>() = block;
  Op<-1>() = inst;
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumIncoming() const
{
  assert((numOps_ & 1) == 0 && "invalid node count");
  return numOps_ >> 1;
}

// -----------------------------------------------------------------------------
const Block *PhiInst::GetBlock(unsigned i) const
{
  return static_cast<Block *>((op_begin() + i * 2 + 0)->get());
}

// -----------------------------------------------------------------------------
void PhiInst::SetBlock(unsigned i, Block *block)
{
  *(op_begin() + i * 2 + 0) = block;
}

// -----------------------------------------------------------------------------
void PhiInst::SetValue(unsigned i, Inst *inst)
{
  *(op_begin() + i * 2 + 1) = inst;
}

// -----------------------------------------------------------------------------
const Inst *PhiInst::GetValue(unsigned i) const
{
  return static_cast<Inst *>((op_begin() + i * 2 + 1)->get());
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
const Inst *PhiInst::GetValue(const Block *block) const
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return GetValue(i);
    }
  }
  llvm_unreachable("invalid predecessor");
}

// -----------------------------------------------------------------------------
bool PhiInst::HasValue(const Block *block)
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return true;
    }
  }
  return false;
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
