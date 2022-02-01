// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/block.h"
#include "core/insts/control.h"



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

