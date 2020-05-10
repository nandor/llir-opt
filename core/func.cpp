// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/func.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/prog.h"


// -----------------------------------------------------------------------------
static unsigned kUniqueID = 0;

// -----------------------------------------------------------------------------
Func::Func(const std::string_view name)
  : Global(Global::Kind::FUNC, name)
  , id_(kUniqueID++)
  , parent_(nullptr)
  , stackSize_(0ull)
  , stackAlign_(1ull)
  , callConv_(CallingConv::C)
  , varArg_(false)
  , align_(16u)
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
void Func::insert(iterator it, Block *block)
{
  blocks_.insert(it, block);
}

// -----------------------------------------------------------------------------
void Func::clear()
{
  stackSize_ = 0;
  objects_.clear();
  objectIndices_.clear();
  blocks_.clear();
}

// -----------------------------------------------------------------------------
bool Func::HasAddressTaken() const
{
  for (const User *user : users()) {
    auto *movInst = ::dyn_cast_or_null<const MovInst>(user);
    if (!movInst) {
      return true;
    }

    for (const User *movUsers : movInst->users()) {
      auto *movUserInst = ::dyn_cast_or_null<const Inst>(movUsers);
      if (!movUserInst) {
        return true;
      }

      switch (movUserInst->GetKind()) {
        case Inst::Kind::CALL:
        case Inst::Kind::TCALL:
        case Inst::Kind::INVOKE:
        case Inst::Kind::TINVOKE: {
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
void Func::AddBlock(Block *block)
{
  blocks_.push_back(block);
}

// -----------------------------------------------------------------------------
unsigned Func::AddStackObject(unsigned index, unsigned size, unsigned align)
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
