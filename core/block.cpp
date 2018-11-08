// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/func.h"
#include "core/insts.h"



// -----------------------------------------------------------------------------
Block::Block(Func *parent, const std::string_view name)
  : parent_(parent)
  , name_(name)
{
}

// -----------------------------------------------------------------------------
void Block::AddInst(Inst *i)
{
  insts_.push_back(i);
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
TerminatorInst *Block::GetTerminator()
{
  if (IsEmpty()) {
    return nullptr;
  } else {
    auto *last = &*insts_.rbegin();
    if (last->IsTerminator()) {
      return static_cast<TerminatorInst *>(last);
    } else {
      return nullptr;
    }
  }
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
