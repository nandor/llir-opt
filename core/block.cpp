// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/func.h"
#include "core/insts.h"



// -----------------------------------------------------------------------------
Block::Block(Func *parent, const std::string_view name)
  : Global(Global::Kind::BLOCK, name, true)
  , parent_(parent)
  , name_(name)
{
}

// -----------------------------------------------------------------------------
Block::~Block()
{
}

// -----------------------------------------------------------------------------
void Block::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
void Block::erase(iterator it)
{
  insts_.erase(it);
}

// -----------------------------------------------------------------------------
void Block::erase(iterator first, iterator last)
{
  for (auto it = first; it != last; ) {
    (*it++).eraseFromParent();
  }
}

// -----------------------------------------------------------------------------
void Block::AddInst(Inst *i, Inst *before)
{
  if (before == nullptr) {
    insts_.push_back(i);
  } else {
    insts_.insert(before->getIterator(), i);
  }
}

// -----------------------------------------------------------------------------
void Block::AddPhi(PhiInst *phi)
{
  insts_.push_front(phi);
}

// -----------------------------------------------------------------------------
Block::succ_iterator Block::succ_begin()
{
  return succ_iterator(GetTerminator());
}

// -----------------------------------------------------------------------------
Block::succ_iterator Block::succ_end()
{
  return succ_iterator(GetTerminator(), true);
}

// -----------------------------------------------------------------------------
Block::const_succ_iterator Block::succ_begin() const
{
  return const_succ_iterator(GetTerminator());
}

// -----------------------------------------------------------------------------
Block::const_succ_iterator Block::succ_end() const
{
  return const_succ_iterator(GetTerminator(), true);
}

// -----------------------------------------------------------------------------
Block::pred_iterator Block::pred_begin()
{
  return pred_iterator(this);
}

// -----------------------------------------------------------------------------
Block::pred_iterator Block::pred_end()
{
  return pred_iterator(this, true);
}

// -----------------------------------------------------------------------------
Block::const_pred_iterator Block::pred_begin() const
{
  return const_pred_iterator(this);
}

// -----------------------------------------------------------------------------
Block::const_pred_iterator Block::pred_end() const
{
  return const_pred_iterator(this, true);
}

// -----------------------------------------------------------------------------
const TerminatorInst *Block::GetTerminator() const
{
  if (IsEmpty()) {
    return nullptr;
  } else {
    auto *last = &*insts_.rbegin();
    if (last->IsTerminator()) {
      return static_cast<const TerminatorInst *>(last);
    } else {
      return nullptr;
    }
  }
}

// -----------------------------------------------------------------------------
TerminatorInst *Block::GetTerminator()
{
  return const_cast<TerminatorInst *>(
      static_cast<const Block *>(this)->GetTerminator()
  );
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Block::phi_iterator> Block::phis()
{
  PhiInst *start;
  if (!IsEmpty() && begin()->Is(Inst::Kind::PHI)) {
    start = static_cast<PhiInst *>(&*begin());
  } else {
    start = nullptr;
  }
  return llvm::make_range<phi_iterator>(start, nullptr);
}

// -----------------------------------------------------------------------------
Block *Block::splitBlock(iterator I)
{
  Block *cont = new Block(parent_, name_ + ".split");
  parent_->insertAfter(getIterator(), cont);

  // Transfer the instructions.
  cont->insts_.splice(cont->end(), insts_, I, insts_.end());

  // Adjust PHIs in the successors of the new block.
  for (auto *succ : cont->successors()) {
    for (auto &phi : succ->phis()) {
      for (unsigned i = 0; i < phi.GetNumIncoming(); ++i) {
        if (phi.GetBlock(i) == this) {
          phi.SetBlock(i, cont);
        }
      }
    }
  }

  return cont;
}

// -----------------------------------------------------------------------------
void Block::printAsOperand(llvm::raw_ostream &O, bool PrintType) const
{
  O << "dummy";
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
  delete inst;
}

// -----------------------------------------------------------------------------
Block *llvm::ilist_traits<Inst>::getParent() {
  auto field = &(static_cast<Block *>(nullptr)->*&Block::insts_);
  auto offset = reinterpret_cast<char *>(field) - static_cast<char *>(nullptr);
  return reinterpret_cast<Block *>(reinterpret_cast<char *>(this) - offset);
}
