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
void Block::printAsOperand(llvm::raw_ostream &O, bool PrintType) const
{
  O << "dummy";
}
