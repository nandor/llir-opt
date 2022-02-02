// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"

#include <sstream>

#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/printer.h"



// -----------------------------------------------------------------------------
Block::Block(const std::string_view name, Visibility visibility)
  : Global(Global::Kind::BLOCK, name, visibility)
  , parent_(nullptr)
{
}

// -----------------------------------------------------------------------------
Block::~Block()
{
}

// -----------------------------------------------------------------------------
void Block::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Block::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
void Block::insert(Inst *i, iterator it)
{
  insts_.insert(it, i);
}

// -----------------------------------------------------------------------------
void Block::insertAfter(Inst *i, iterator it)
{
  insts_.insertAfter(it, i);
}

// -----------------------------------------------------------------------------
void Block::remove(iterator it)
{
  insts_.remove(it);
}

// -----------------------------------------------------------------------------
void Block::erase(iterator it)
{
  insts_.erase(it);
}

// -----------------------------------------------------------------------------
void Block::clear()
{
  insts_.clear();
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
const TerminatorInst *Block::GetTerminator() const
{
  if (empty()) {
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
bool Block::HasAddressTaken() const
{
  for (const User *user : users()) {
    auto *inst = ::cast_or_null<const Inst>(user);
    if (!inst || inst->Is(Inst::Kind::MOV)) {
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
bool Block::IsTrap() const
{
  if (size() != 1) {
    return false;
  }
  switch (GetTerminator()->GetKind()) {
    default: {
      return false;
    }
    case Inst::Kind::TRAP:
    case Inst::Kind::DEBUG_TRAP: {
      return true;
    }
  }
}

// -----------------------------------------------------------------------------
bool Block::IsLandingPad() const
{
  for (const Block *bb : predecessors()) {
    if (auto *invoke = ::cast_or_null<const InvokeInst>(bb->GetTerminator())) {
      if (bb == invoke->GetThrow()) {
        return true;
      }
    }
  }
  return false;
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
  if (!empty() && begin()->Is(Inst::Kind::PHI)) {
    start = static_cast<PhiInst *>(&*begin());
  } else {
    start = nullptr;
  }
  return llvm::make_range<phi_iterator>(start, nullptr);
}

// -----------------------------------------------------------------------------
Block::iterator Block::first_non_phi()
{
  iterator it = begin();
  while (it != end() && it->Is(Inst::Kind::PHI)) {
    ++it;
  }
  return it;
}

// -----------------------------------------------------------------------------
Block *Block::splitBlock(iterator I)
{
  static unsigned uniqueID = 0;
  std::ostringstream os;
  os << GetName() << ".split$" << uniqueID++;
  Block *cont = new Block(os.str());
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
  O << getName();
}

// -----------------------------------------------------------------------------
Prog *Block::getProg()
{
  return getParent()->getParent();
}

// -----------------------------------------------------------------------------
void Block::dump(llvm::raw_ostream &os) const
{
  Printer(os).Print(*this);
}
