// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts/control.h"



// -----------------------------------------------------------------------------
ReturnInst::ReturnInst(llvm::ArrayRef<Inst *> values, AnnotSet &&annot)
  : TerminatorInst(Kind::RET, values.size(), std::move(annot))
{
  for (unsigned i = 0, n = values.size(); i < n; ++i) {
    *(this->op_begin() + i) = values[i];
  }
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
Inst *ReturnInst::arg(unsigned i) const
{
  return static_cast<Inst *>((this->op_begin() + i)->get());
}

// -----------------------------------------------------------------------------
size_t ReturnInst::arg_size() const
{
  return size();
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
RaiseInst::RaiseInst(
    Inst *target,
    Inst *stack,
    llvm::ArrayRef<Inst *> values,
    AnnotSet &&annot)
  : TerminatorInst(Kind::RAISE, 2 + values.size(), std::move(annot))
{
  Op<0>() = target;
  Op<1>() = stack;
  for (unsigned i = 0, n = values.size(); i < n; ++i) {
    *(this->op_begin() + i + 2) = values[i];
  }
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
Inst *RaiseInst::arg(unsigned i) const
{
  return static_cast<Inst *>((this->op_begin() + i + 2)->get());
}

// -----------------------------------------------------------------------------
size_t RaiseInst::arg_size() const
{
  return size() - 2;
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
