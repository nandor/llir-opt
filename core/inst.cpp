// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/inst.h"
#include "core/insts.h"


// -----------------------------------------------------------------------------
static int InstructionID = 0;


// -----------------------------------------------------------------------------
Inst::Inst(Kind kind, unsigned numOps, const AnnotSet &annot)
  : User(Value::Kind::INST, numOps)
  , kind_(kind)
  , annot_(annot)
  , parent_(nullptr)
  , order_(++InstructionID)
{
}

// -----------------------------------------------------------------------------
Inst::~Inst()
{
}

// -----------------------------------------------------------------------------
void Inst::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Inst::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
unsigned TerminatorInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type TerminatorInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
unsigned OperatorInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type OperatorInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
UnaryInst::UnaryInst(
    Kind kind,
    Type type,
    Inst *arg,
    const AnnotSet &annot)
  : OperatorInst(kind, type, 1, annot)
{
  Op<0>() = arg;
}

// -----------------------------------------------------------------------------
Inst *UnaryInst::GetArg() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
BinaryInst::BinaryInst(
    Kind kind,
    Type type,
    Inst *lhs,
    Inst *rhs,
    const AnnotSet &annot)
  : OperatorInst(kind, type, 2, annot)
{
  Op<0>() = lhs;
  Op<1>() = rhs;
}

// -----------------------------------------------------------------------------
Inst *BinaryInst::GetLHS() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
Inst *BinaryInst::GetRHS() const
{
  return static_cast<Inst *>(Op<1>().get());
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Inst>::addNodeToList(Inst *inst)
{
  inst->setParent(getParent());
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Inst>::removeNodeFromList(Inst *inst)
{
  inst->setParent(nullptr);
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Inst>::transferNodesFromList(
    ilist_traits &from,
    instr_iterator first,
    instr_iterator last)
{
  Block *parent = getParent();
  for (auto it = first; it != last; ++it) {
    it->setParent(parent);
  }
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Inst>::deleteNode(Inst *inst)
{
  inst->replaceAllUsesWith(nullptr);
  delete inst;
}

// -----------------------------------------------------------------------------
Block *llvm::ilist_traits<Inst>::getParent() {
  auto field = &(static_cast<Block *>(nullptr)->*&Block::insts_);
  auto offset = reinterpret_cast<char *>(field) - static_cast<char *>(nullptr);
  return reinterpret_cast<Block *>(reinterpret_cast<char *>(this) - offset);
}
