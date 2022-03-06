// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/func.h"

#include "core/block.h"
#include "core/cast.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/printer.h"



// -----------------------------------------------------------------------------
static unsigned kUniqueID = 0;

// -----------------------------------------------------------------------------
Func::Func(const std::string_view name, Visibility visibility)
  : Global(Global::Kind::FUNC, name, visibility)
  , id_(kUniqueID++)
  , parent_(nullptr)
  , callConv_(CallingConv::C)
  , varArg_(false)
  , align_(std::nullopt)
  , noinline_(false)
{
}

// -----------------------------------------------------------------------------
Func::~Func()
{
}

// -----------------------------------------------------------------------------
void Func::SetPersonality(Global *func)
{
  resizeUses(1);
  Set(0, func);
}

// -----------------------------------------------------------------------------
ConstRef<Global> Func::GetPersonality() const
{
  if (User::size() != 0) {
    return ::cast<const Global>(Get(0));
  } else {
    return nullptr;
  }
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
      auto *site = ::cast_or_null<const CallSite>(movUsers);
      if (!site) {
        return true;
      }

      for (ConstRef<Inst> arg : site->args()) {
        if (arg.Get() == movInst) {
          return true;
        }
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool Func::DoesNotReturn() const
{
  for (const Block &block : *this) {
    for (const Inst &inst : block) {
      if (inst.IsReturn()) {
        return false;
      }
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
bool Func::HasRaise() const
{
  for (const Block &block : *this) {
    for (const Inst &inst : block) {
      if (inst.Is(Inst::Kind::RAISE)) {
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool Func::HasVAStart() const
{
  for (const Block &block : *this) {
    for (const Inst &inst : block) {
      if (inst.Is(Inst::Kind::VA_START)) {
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool Func::HasIndirectCalls() const
{
  for (const Block &block : *this) {
    if (auto *call = ::cast_or_null<const CallSite>(block.GetTerminator())) {
      if (!call->GetDirectCallee()) {
        return true;
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
      llvm::DenseSet<Block *> succs;
      for (auto *succ : block.successors()) {
        succs.insert(succ);
      }
      for (Block *succ : succs) {
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

// -----------------------------------------------------------------------------
void Func::dump(llvm::raw_ostream &os) const
{
  Printer(os).Print(*this);
}
