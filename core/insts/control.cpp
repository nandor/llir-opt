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
  : TerminatorInst(Kind::JCC, 3, std::move(annot))
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
  : TerminatorInst(Kind::JCC, 3, annot)
{
  Set<0>(cond);
  Set<1>(bt);
  Set<2>(bf);
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
  : TerminatorInst(Kind::JMP, 1, std::move(annot))
{
  Set<0>(target);
}

// -----------------------------------------------------------------------------
JumpInst::JumpInst(Block *target, const AnnotSet &annot)
  : TerminatorInst(Kind::JMP, 1, annot)
{
  Set<0>(target);
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
ReturnInst::ReturnInst(llvm::ArrayRef<Ref<Inst>> values, AnnotSet &&annot)
  : TerminatorInst(Kind::RET, values.size(), std::move(annot))
{
  for (unsigned i = 0, n = values.size(); i < n; ++i) {
    Set(i, values[i]);
  }
}

// -----------------------------------------------------------------------------
Block *ReturnInst::getSuccessor(unsigned i)
{
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned ReturnInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
size_t ReturnInst::arg_size() const
{
  return size();
}

// -----------------------------------------------------------------------------
Ref<Inst> ReturnInst::arg(unsigned i)
{
  return cast<Inst>(Get(i));
}


// -----------------------------------------------------------------------------
RaiseInst::RaiseInst(
    std::optional<CallingConv> conv,
    Ref<Inst> target,
    Ref<Inst> stack,
    llvm::ArrayRef<Ref<Inst>> values,
    AnnotSet &&annot)
  : TerminatorInst(Kind::RAISE, 2 + values.size(), std::move(annot))
  , conv_(conv)
{
  Set<0>(target);
  Set<1>(stack);
  for (unsigned i = 0, n = values.size(); i < n; ++i) {
    Set(i + 2, values[i]);
  }
}

// -----------------------------------------------------------------------------
Block *RaiseInst::getSuccessor(unsigned i)
{
  llvm_unreachable("RaiseInst terminator has no successors");
}

// -----------------------------------------------------------------------------
unsigned RaiseInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
size_t RaiseInst::arg_size() const
{
  return size() - 2;
}

// -----------------------------------------------------------------------------
ConstRef<Inst> RaiseInst::GetTarget() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> RaiseInst::GetTarget()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> RaiseInst::GetStack() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> RaiseInst::GetStack()
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> RaiseInst::arg(unsigned i)
{
  return cast<Inst>(Get(i + 2));
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
ConstRef<Inst> SwitchInst::GetIdx() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> SwitchInst::GetIdx()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
TrapInst::TrapInst(AnnotSet &&annot)
  : TerminatorInst(Kind::TRAP, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
TrapInst::TrapInst(const AnnotSet &annot)
  : TerminatorInst(Kind::TRAP, 0, annot)
{
}

// -----------------------------------------------------------------------------
Block *TrapInst::getSuccessor(unsigned i)
{
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned TrapInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
LandingPadInst::LandingPadInst(
    std::optional<CallingConv> conv,
    llvm::ArrayRef<Type> types,
    AnnotSet &&annot)
  : Inst(Inst::Kind::LANDING_PAD, 0, std::move(annot))
  , conv_(conv)
  , types_(types)
{
}

// -----------------------------------------------------------------------------
LandingPadInst::LandingPadInst(
    std::optional<CallingConv> conv,
    llvm::ArrayRef<Type> types,
    const AnnotSet &annot)
  : Inst(Inst::Kind::LANDING_PAD, 0, annot)
  , conv_(conv)
  , types_(types)
{
}

// -----------------------------------------------------------------------------
Type LandingPadInst::GetType(unsigned i) const
{
  return types_[i];
}
