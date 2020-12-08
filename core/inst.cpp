// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/inst.h"
#include "core/insts.h"

// -----------------------------------------------------------------------------
static int InstructionID = 0;



// -----------------------------------------------------------------------------
Ref<Inst> Inst::arg_iterator::operator*() const
{
  return ::cast<Inst>(*this->I);
}

// -----------------------------------------------------------------------------
Ref<Inst> Inst::arg_iterator::operator->() const
{
  return ::cast<Inst>(*this->I);
}

// -----------------------------------------------------------------------------
ConstRef<Inst> Inst::const_arg_iterator::operator*() const
{
  return ::cast<Inst>(*this->I);
}

// -----------------------------------------------------------------------------
ConstRef<Inst> Inst::const_arg_iterator::operator->() const
{
  return ::cast<Inst>(*this->I);
}

// -----------------------------------------------------------------------------
Ref<Block> Inst::block_iterator::operator*() const
{
  return ::cast<Block>(*this->I);
}

// -----------------------------------------------------------------------------
Ref<Block> Inst::block_iterator::operator->() const
{
  return ::cast<Block>(*this->I);
}

// -----------------------------------------------------------------------------
Inst::Inst(Kind kind, unsigned numOps, AnnotSet &&annot)
  : User(Value::Kind::INST, numOps)
  , kind_(kind)
  , annot_(std::move(annot))
  , parent_(nullptr)
  , order_(++InstructionID)
{
}

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
void Inst::replaceAllUsesWith(Value *v)
{
  auto it = use_begin();
  while (it != use_end()) {
    Use &use = *it;
    ++it;
    use = Ref<Value>(v, (*use).Index());
  }
}

// -----------------------------------------------------------------------------
void Inst::replaceAllUsesWith(llvm::ArrayRef<Ref<Inst>> v)
{
  assert(GetNumRets() == v.size() && "invalid number of return values");
  auto it = use_begin();
  while (it != use_end()) {
    Use &use = *it;
    ++it;
    use = v[(*use).Index()];
  }
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
    Ref<Inst> arg,
    AnnotSet &&annot)
  : OperatorInst(kind, type, 1, std::move(annot))
{
  Set<0>(arg);
}

// -----------------------------------------------------------------------------
ConstRef<Inst> UnaryInst::GetArg() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> UnaryInst::GetArg()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
BinaryInst::BinaryInst(
    Kind kind,
    Type type,
    Ref<Inst> lhs,
    Ref<Inst> rhs,
    AnnotSet &&annot)
  : OperatorInst(kind, type, 2, std::move(annot))
{
  Set<0>(lhs);
  Set<1>(rhs);
}

// -----------------------------------------------------------------------------
ConstRef<Inst> BinaryInst::GetLHS() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> BinaryInst::GetLHS()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> BinaryInst::GetRHS() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> BinaryInst::GetRHS()
{
  return cast<Inst>(Get<1>());
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
