// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts/control.h"



// -----------------------------------------------------------------------------
ReturnInst::ReturnInst(AnnotSet &&annot)
  : TerminatorInst(Kind::RET, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
ReturnInst::ReturnInst(Inst *op, AnnotSet &&annot)
  : TerminatorInst(Kind::RET, 1, std::move(annot))
{
  Op<0>() = op;
}

// -----------------------------------------------------------------------------
Block *ReturnInst::getSuccessor(unsigned i) const
{
  llvm_unreachable("invalid successor");
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
JumpCondInst::JumpCondInst(
    Value *cond,
    Block *bt,
    Block *bf,
    AnnotSet &&annot)
  : TerminatorInst(Kind::JCC, 3, std::move(annot))
{
  Op<0>() = cond;
  Op<1>() = bt;
  Op<2>() = bf;
}

// -----------------------------------------------------------------------------
JumpCondInst::JumpCondInst(
    Value *cond,
    Block *bt,
    Block *bf,
    const AnnotSet &annot)
  : TerminatorInst(Kind::JCC, 3, annot)
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
  llvm_unreachable("invalid successor");
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
RaiseInst::RaiseInst(Inst *target, Inst *stack, AnnotSet &&annot)
  : TerminatorInst(Kind::RAISE, 2, std::move(annot))
{
  Op<0>() = target;
  Op<1>() = stack;
}

// -----------------------------------------------------------------------------
Block *RaiseInst::getSuccessor(unsigned i) const
{
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned RaiseInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
ReturnJumpInst::ReturnJumpInst(
    Inst *target,
    Inst *stack,
    Inst *value,
    AnnotSet &&annot)
  : TerminatorInst(Kind::RETJMP, 3, std::move(annot))
{
  Op<0>() = target;
  Op<1>() = stack;
  Op<2>() = value;
}

// -----------------------------------------------------------------------------
Block *ReturnJumpInst::getSuccessor(unsigned i) const
{
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned ReturnJumpInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
JumpInst::JumpInst(Block *target, AnnotSet &&annot)
  : TerminatorInst(Kind::JMP, 1, std::move(annot))
{
  Op<0>() = target;
}

// -----------------------------------------------------------------------------
JumpInst::JumpInst(Block *target, const AnnotSet &annot)
  : TerminatorInst(Kind::JMP, 1, annot)
{
  Op<0>() = target;
}

// -----------------------------------------------------------------------------
Block *JumpInst::getSuccessor(unsigned i) const
{
  if (i == 0) return static_cast<Block *>(Op<0>().get());
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned JumpInst::getNumSuccessors() const
{
  return 1;
}

// -----------------------------------------------------------------------------
SwitchInst::SwitchInst(
    Inst *index,
    const std::vector<Block *> &branches,
    AnnotSet &&annot)
  : TerminatorInst(Kind::SWITCH, branches.size() + 1, std::move(annot))
{
  Op<0>() = index;
  for (unsigned i = 0, n = branches.size(); i < n; ++i) {
    *(op_begin() + i + 1) = branches[i];
  }
}

// -----------------------------------------------------------------------------
SwitchInst::SwitchInst(
    Inst *index,
    const std::vector<Block *> &branches,
    const AnnotSet &annot)
  : TerminatorInst(Kind::SWITCH, branches.size() + 1, annot)
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
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned SwitchInst::getNumSuccessors() const
{
  return numOps_ - 1;
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
Block *TrapInst::getSuccessor(unsigned i) const
{
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned TrapInst::getNumSuccessors() const
{
  return 0;
}
