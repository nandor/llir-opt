// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/block.h"
#include "core/insts/control.h"



// -----------------------------------------------------------------------------
JumpCondInst::JumpCondInst(
    Ref<Inst> cond,
    Block *bt,
    Block *bf,
    AnnotSet &&annot)
  : TerminatorInst(Kind::JUMP_COND, 3, std::move(annot))
{
  Set<0>(cond);
  Set<1>(bt);
  Set<2>(bf);
}

// -----------------------------------------------------------------------------
JumpCondInst::JumpCondInst(
    Ref<Inst> cond,
    Block *bt,
    Block *bf,
    const AnnotSet &annot)
  : TerminatorInst(Kind::JUMP_COND, 3, annot)
{
  Set<0>(cond);
  Set<1>(bt);
  Set<2>(bf);
}

// -----------------------------------------------------------------------------
const Block *JumpCondInst::getSuccessor(unsigned i) const
{
  if (i == 0) return GetTrueTarget();
  if (i == 1) return GetFalseTarget();
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::getSuccessor(unsigned i)
{
  if (i == 0) return GetTrueTarget();
  if (i == 1) return GetFalseTarget();
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned JumpCondInst::getNumSuccessors() const
{
  return 2;
}

// -----------------------------------------------------------------------------
ConstRef<Inst> JumpCondInst::GetCond() const
{
  return cast<Inst>(Get<0>()).Get();
}

// -----------------------------------------------------------------------------
Ref<Inst> JumpCondInst::GetCond()
{
  return cast<Inst>(Get<0>()).Get();
}

// -----------------------------------------------------------------------------
const Block *JumpCondInst::GetTrueTarget() const
{
  return cast<Block>(Get<1>()).Get();
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::GetTrueTarget()
{
  return cast<Block>(Get<1>()).Get();
}

// -----------------------------------------------------------------------------
const Block *JumpCondInst::GetFalseTarget() const
{
  return cast<Block>(Get<2>()).Get();
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::GetFalseTarget()
{
  return cast<Block>(Get<2>()).Get();
}

// -----------------------------------------------------------------------------
JumpInst::JumpInst(Block *target, AnnotSet &&annot)
  : TerminatorInst(Kind::JUMP, 1, std::move(annot))
{
  Set<0>(target);
}

// -----------------------------------------------------------------------------
JumpInst::JumpInst(Block *target, const AnnotSet &annot)
  : TerminatorInst(Kind::JUMP, 1, annot)
{
  Set<0>(target);
}

// -----------------------------------------------------------------------------
const Block *JumpInst::getSuccessor(unsigned i) const
{
  if (i == 0) return GetTarget();
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
Block *JumpInst::getSuccessor(unsigned i)
{
  if (i == 0) return GetTarget();
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned JumpInst::getNumSuccessors() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Block *JumpInst::GetTarget() const
{
  return cast<Block>(Get<0>()).Get();
}

// -----------------------------------------------------------------------------
Block *JumpInst::GetTarget()
{
  return cast<Block>(Get<0>()).Get();
}

// -----------------------------------------------------------------------------
SwitchInst::SwitchInst(
    Ref<Inst> index,
    llvm::ArrayRef<Block *> branches,
    AnnotSet &&annot)
  : TerminatorInst(Kind::SWITCH, branches.size() + 1, std::move(annot))
{
  Set<0>(index);
  for (unsigned i = 0, n = branches.size(); i < n; ++i) {
    Set(i + 1, branches[i]);
  }
}

// -----------------------------------------------------------------------------
SwitchInst::SwitchInst(
    Ref<Inst> index,
    llvm::ArrayRef<Block *> branches,
    const AnnotSet &annot)
  : TerminatorInst(Kind::SWITCH, branches.size() + 1, annot)
{
  Set<0>(index);
  for (unsigned i = 0, n = branches.size(); i < n; ++i) {
    Set(i + 1, branches[i]);
  }
}

// -----------------------------------------------------------------------------
SwitchInst::SwitchInst(
    Ref<Inst> index,
    llvm::ArrayRef<Ref<Block>> branches,
    const AnnotSet &annot)
  : TerminatorInst(Kind::SWITCH, branches.size() + 1, annot)
{
  Set<0>(index);
  for (unsigned i = 0, n = branches.size(); i < n; ++i) {
    Set(i + 1, branches[i]);
  }
}

// -----------------------------------------------------------------------------
const Block *SwitchInst::getSuccessor(unsigned i) const
{
  if (i + 1 < numOps_) return cast<Block>(Get(i + 1)).Get();
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
Block *SwitchInst::getSuccessor(unsigned i)
{
  if (i + 1 < numOps_) return cast<Block>(Get(i + 1)).Get();
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned SwitchInst::getNumSuccessors() const
{
  return numOps_ - 1;
}

// -----------------------------------------------------------------------------
ConstRef<Inst> SwitchInst::GetIndex() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> SwitchInst::GetIndex()
{
  return cast<Inst>(Get<0>());
}

