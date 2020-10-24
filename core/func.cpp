// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/func.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/prog.h"
#include "core/insts/const.h"
#include "core/insts/phi.h"



// -----------------------------------------------------------------------------
static unsigned kUniqueID = 0;

// -----------------------------------------------------------------------------
Func::Func(const std::string_view name, Visibility visibility)
  : Global(Global::Kind::FUNC, name, visibility)
  , id_(kUniqueID++)
  , parent_(nullptr)
  , callConv_(CallingConv::C)
  , varArg_(false)
  , align_(1u)
  , noinline_(false)
{
}

// -----------------------------------------------------------------------------
Func::~Func()
{
}

// -----------------------------------------------------------------------------
void Func::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Func::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
void Func::remove(iterator it)
{
  blocks_.remove(it);
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
void Func::insert(iterator it, Block *block)
{
  blocks_.insert(it, block);
}

// -----------------------------------------------------------------------------
void Func::clear()
{
  params_.clear();
  objects_.clear();
  objectIndices_.clear();
  blocks_.clear();
}

// -----------------------------------------------------------------------------
bool Func::HasAddressTaken() const
{
  for (const User *user : users()) {
    auto *movInst = ::cast_or_null<const MovInst>(user);
    if (!movInst) {
      return true;
    }

    for (const User *movUsers : movInst->users()) {
      auto *movUserInst = ::cast_or_null<const Inst>(movUsers);
      if (!movUserInst) {
        return true;
      }

      switch (movUserInst->GetKind()) {
        case Inst::Kind::CALL:
        case Inst::Kind::TCALL:
        case Inst::Kind::INVOKE: {
          continue;
        }
        default: {
          return true;
        }
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
Block &Func::getEntryBlock()
{
  return *begin();
}

// -----------------------------------------------------------------------------
void Func::AddBlock(Block *block, Block *before)
{
  if (before == nullptr) {
    blocks_.push_back(block);
  } else {
    blocks_.insert(before->getIterator(), block);
  }
}

// -----------------------------------------------------------------------------
unsigned Func::AddStackObject(unsigned index, unsigned size, llvm::Align align)
{
  auto it = objectIndices_.insert({index, objects_.size()});
  if (!it.second) {
    llvm::report_fatal_error("Duplicate stack object");
  }
  objects_.emplace_back(index, size, align);
  return index;
}

// -----------------------------------------------------------------------------
void Func::RemoveStackObject(unsigned index)
{
  auto it = objectIndices_.find(index);
  assert(it != objectIndices_.end() && "missing stack object");
  objectIndices_.erase(it);

  for (auto it = objects_.begin(); it != objects_.end(); ) {
    if (it->Index == index) {
      it = objects_.erase(it);
    } else {
      it++;
    }
  }
}

// -----------------------------------------------------------------------------
size_t Func::inst_size() const
{
  size_t i = 0;
  for (const Block &block : *this) {
    i += block.size();
  }
  return i;
}

// -----------------------------------------------------------------------------
void Func::RemoveUnreachable()
{
  llvm::SmallPtrSet<Block *, 10> blocks;

  std::function<void(Block *)> dfs = [&blocks, &dfs] (Block *block) {
    if (!blocks.insert(block).second) {
      return;
    }
    for (auto *succ : block->successors()) {
      dfs(succ);
    }
  };

  dfs(&getEntryBlock());

  for (auto &block : blocks_) {
    if (blocks.count(&block) == 0) {
      for (auto *succ : block.successors()) {
        for (auto &phi : succ->phis()) {
          phi.Remove(&block);
        }
      }
    }
  }

  for (auto it = begin(); it != end(); ) {
    Block *block = &*it++;
    if (blocks.count(block) == 0) {
      block->replaceAllUsesWith(new ConstantInt(0));
      block->eraseFromParent();
    }
  }
}
