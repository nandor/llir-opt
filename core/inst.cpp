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

/*
#define GET_BASE_IMPL
#include "instructions.def"
*/
