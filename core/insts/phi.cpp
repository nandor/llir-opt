// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts.h"



// -----------------------------------------------------------------------------
PhiInst::PhiInst(Type type, AnnotSet &&annot)
  : Inst(Kind::PHI, 0, std::move(annot))
  , type_(type)
{
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
void PhiInst::Add(Block *block, Ref<Inst> inst)
{
  for (unsigned i = 0, n = GetNumIncoming(); i < n; ++i) {
    if (GetBlock(i) == block) {
      Set(i * 2 + 1, inst);
      return;
    }
  }
  resizeUses(numOps_ + 2);
  Set<-2>(block);
  Set<-1>(inst);
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumIncoming() const
{
  assert((numOps_ & 1) == 0 && "invalid node count");
  return numOps_ / 2;
}

// -----------------------------------------------------------------------------
void PhiInst::Remove(const Block *block)
{
  for (unsigned i = 0, n = GetNumIncoming(); i < n; ++i) {
    if (GetBlock(i) == block) {
      if (i != n - 1) {
        Set(i * 2 + 0, GetBlock(n - 1));
        Set(i * 2 + 1, GetValue(n - 1));
      }
      resizeUses(numOps_ - 2);
      return;
    }
  }
  llvm_unreachable("invalid predecessor");
}

// -----------------------------------------------------------------------------
void PhiInst::SetBlock(unsigned i, Block *block)
{
  Set(i * 2 + 0, block);
}

// -----------------------------------------------------------------------------
const Block *PhiInst::GetBlock(unsigned i) const
{
  return cast<Block>(Get(i * 2 + 0)).Get();
}

// -----------------------------------------------------------------------------
Block *PhiInst::GetBlock(unsigned i)
{
  return cast<Block>(Get(i * 2 + 0)).Get();
}

// -----------------------------------------------------------------------------
void PhiInst::SetValue(unsigned i, Ref<Inst> value)
{
  Set(i * 2 + 1, value);
}

// -----------------------------------------------------------------------------
ConstRef<Inst> PhiInst::GetValue(unsigned i) const
{
  return cast<Inst>(Get(i * 2 + 1));
}

// -----------------------------------------------------------------------------
Ref<Inst> PhiInst::GetValue(unsigned i)
{
  return cast<Inst>(Get(i * 2 + 1));
}

// -----------------------------------------------------------------------------
Ref<Inst> PhiInst::GetValue(const Block *block)
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return GetValue(i);
    }
  }
  llvm_unreachable("invalid predecessor");
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
