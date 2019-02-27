// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/func.h"
#include "core/block.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Func::Func(Prog *prog, const std::string_view name)
  : Global(Global::Kind::FUNC, name, true)
  , prog_(prog)
  , stackSize_(0ull)
  , callConv_(CallingConv::C)
  , varArg_(false)
  , align_(0u)
  , visibility_(Visibility::EXTERN)
  , noinline_(false)
{
}

// -----------------------------------------------------------------------------
Func::~Func()
{
}

// -----------------------------------------------------------------------------
void Func::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
void Func::erase(iterator it)
{
  blocks_.erase(it);
}

// -----------------------------------------------------------------------------
void Func::insertAfter(iterator it, Block *block)
{
  blocks_.insertAfter(it, block);
}

// -----------------------------------------------------------------------------
void Func::AddBlock(Block *block)
{
  blocks_.push_back(block);
}

// -----------------------------------------------------------------------------
void Func::SetStackSize(size_t stackSize)
{
  stackSize_ = stackSize;
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Block>::addNodeToList(Block *block)
{
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Block>::removeNodeFromList(Block *block)
{
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Block>::transferNodesFromList(
    ilist_traits &from,
    instr_iterator first,
    instr_iterator last)
{
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Block>::deleteNode(Block *block)
{
  block->replaceAllUsesWith(nullptr);
  delete block;
}
