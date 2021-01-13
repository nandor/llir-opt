// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/adt/hash.h"
#include "core/atom.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/global.h"
#include "core/insts.h"
#include "passes/pre_eval/symbolic_pointer.h"



// -----------------------------------------------------------------------------
bool SymbolicAddress::operator==(const SymbolicAddress &that) const
{
  if (v_.K != that.v_.K) {
    return false;
  }

  switch (v_.K) {
    case Kind::ATOM: {
      return v_.A.Symbol == that.v_.A.Symbol
          && v_.A.Offset == that.v_.A.Offset;
    }
    case Kind::ATOM_RANGE: {
      llvm_unreachable("not implemented");
    }
    case Kind::FRAME: {
      llvm_unreachable("not implemented");
    }
    case Kind::FRAME_RANGE: {
      llvm_unreachable("not implemented");
    }
    case Kind::HEAP: {
      return v_.H.Alloc == that.v_.H.Alloc
          && v_.H.Frame == that.v_.H.Frame
          && v_.H.Offset == that.v_.H.Offset;
    }
    case Kind::HEAP_RANGE: {
      llvm_unreachable("not implemented");
    }
    case Kind::EXTERN: {
      llvm_unreachable("not implemented");
    }
    case Kind::EXTERN_RANGE: {
      return v_.ER.Symbol == that.v_.ER.Symbol;
    }
    case Kind::FUNC: {
      return v_.Fn.Fn == that.v_.Fn.Fn;
    }
    case Kind::BLOCK: {
      return v_.B.B == that.v_.B.B;
    }
    case Kind::STACK: {
      return v_.Stk.Frame == that.v_.Stk.Frame;
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
void SymbolicAddress::dump(llvm::raw_ostream &os) const
{
  switch (v_.K) {
    case Kind::ATOM: {
      os << v_.A.Symbol->getName() << " + " << v_.A.Offset;
      return;
    }
    case Kind::ATOM_RANGE: {
      os << v_.AR.Symbol->getName();
      return;
    }
    case Kind::FRAME: {
      os << "<" << v_.F.Frame << ":" << v_.F.Object << "> + " << v_.F.Offset;
      return;
    }
    case Kind::FRAME_RANGE: {
      os << "<" << v_.FR.Frame << ":" << v_.FR.Object << ">";
      return;
    }
    case Kind::HEAP: {
      os << "<" << v_.H.Alloc->getParent()->getName() << "> + " << v_.H.Offset;
      return;
    }
    case Kind::HEAP_RANGE: {
      os << "<" << v_.HR.Alloc->getParent()->getName() << ">";
      return;
    }
    case Kind::EXTERN: {
      os << v_.E.Symbol->getName() << " + " << v_.E.Offset;
      return;
    }
    case Kind::EXTERN_RANGE: {
      os << v_.ER.Symbol->getName();
      return;
    }
    case Kind::FUNC: {
      os << v_.Fn.Fn->getName();
      return;
    }
    case Kind::BLOCK: {
      os << v_.B.B->getName();
      return;
    }
    case Kind::STACK: {
      os << "<" << v_.Stk.Frame << ">";
      return;
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
bool SymbolicAddress::IsPrecise() const
{
  switch (GetKind()) {
    case Kind::ATOM:
    case Kind::FRAME:
    case Kind::HEAP:
    case Kind::EXTERN:
    case Kind::FUNC:
    case Kind::BLOCK:
    case Kind::STACK: {
      return true;
    }
    case Kind::ATOM_RANGE:
    case Kind::FRAME_RANGE:
    case Kind::HEAP_RANGE:
    case Kind::EXTERN_RANGE: {
      return false;
    }
  }
}

// -----------------------------------------------------------------------------
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// -----------------------------------------------------------------------------
SymbolicPointer::address_iterator &
SymbolicPointer::address_iterator::operator++()
{
  std::visit(overloaded {
    [this] (AtomMap::const_iterator it) {
      if (++it != pointer_->atomPointers_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->atomRanges_.empty()) {
        it_ = pointer_->atomRanges_.begin();
        current_.emplace(*pointer_->atomRanges_.begin());
        return;
      }
      if (!pointer_->framePointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->frameRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->heapPointers_.empty()) {
        it_ = pointer_->heapPointers_.begin();
        current_.emplace(*pointer_->heapPointers_.begin());
        return;
      }
      if (!pointer_->heapRanges_.empty()) {
        it_ = pointer_->heapRanges_.begin();
        current_.emplace(*pointer_->heapRanges_.begin());
        return;
      }
      if (!pointer_->externPointers_.empty()) {
        it_ = pointer_->externPointers_.begin();
        current_.emplace(*pointer_->externPointers_.begin());
        return;
      }
      if (!pointer_->externRanges_.empty()) {
        it_ = pointer_->externRanges_.begin();
        current_.emplace(*pointer_->externRanges_.begin());
        return;
      }
      if (!pointer_->heapRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(*pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(*pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (AtomRangeMap::const_iterator it) {
      if (++it != pointer_->atomRanges_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->framePointers_.empty()) {
        it_ = pointer_->framePointers_.begin();
        current_.emplace(*pointer_->framePointers_.begin());
        return;
      }
      if (!pointer_->frameRanges_.empty()) {
        it_ = pointer_->frameRanges_.begin();
        current_.emplace(*pointer_->frameRanges_.begin());
        return;
      }
      if (!pointer_->heapPointers_.empty()) {
        it_ = pointer_->heapPointers_.begin();
        current_.emplace(*pointer_->heapPointers_.begin());
        return;
      }
      if (!pointer_->heapRanges_.empty()) {
        it_ = pointer_->heapRanges_.begin();
        current_.emplace(*pointer_->heapRanges_.begin());
        return;
      }
      if (!pointer_->externPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->externRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(*pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(*pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (FrameMap::const_iterator it) {
      if (++it != pointer_->framePointers_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->frameRanges_.empty()) {
        it_ = pointer_->frameRanges_.begin();
        current_.emplace(*pointer_->frameRanges_.begin());
        return;
      }
      if (!pointer_->heapPointers_.empty()) {
        it_ = pointer_->heapPointers_.begin();
        current_.emplace(*pointer_->heapPointers_.begin());
        return;
      }
      if (!pointer_->heapRanges_.empty()) {
        it_ = pointer_->heapRanges_.begin();
        current_.emplace(*pointer_->heapRanges_.begin());
        return;
      }
      if (!pointer_->externPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->externRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(*pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(*pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (FrameRangeMap::const_iterator it) {
      if (++it != pointer_->frameRanges_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->heapPointers_.empty()) {
        it_ = pointer_->heapPointers_.begin();
        current_.emplace(*pointer_->heapPointers_.begin());
        return;
      }
      if (!pointer_->heapRanges_.empty()) {
        it_ = pointer_->heapRanges_.begin();
        current_.emplace(*pointer_->heapRanges_.begin());
        return;
      }
      if (!pointer_->externPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->externRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(*pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(*pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (HeapMap::const_iterator it) {
      if (++it != pointer_->heapPointers_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->heapRanges_.empty()) {
        it_ = pointer_->heapRanges_.begin();
        current_.emplace(*pointer_->heapRanges_.begin());
        return;
      }
      if (!pointer_->externPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->externRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(*pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(*pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (HeapRangeMap::const_iterator it) {
      if (++it != pointer_->heapRanges_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->externPointers_.empty()) {
        it_ = pointer_->externPointers_.begin();
        current_.emplace(*pointer_->externPointers_.begin());
        return;
      }
      if (!pointer_->externRanges_.empty()) {
        it_ = pointer_->externRanges_.begin();
        current_.emplace(*pointer_->externRanges_.begin());
        return;
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(*pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(*pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (ExternMap::const_iterator it) {
      if (++it != pointer_->externPointers_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->externRanges_.empty()) {
        it_ = pointer_->externRanges_.begin();
        current_.emplace(*pointer_->externRanges_.begin());
        return;
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(*pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(*pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (ExternRangeMap::const_iterator it) {
      if (++it != pointer_->externRanges_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(*pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(*pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (FuncMap::const_iterator it) {
      if (++it != pointer_->funcPointers_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(*pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (BlockMap::const_iterator it) {
      if (++it != pointer_->blockPointers_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(*pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (StackMap::const_iterator it) {
      if (++it != pointer_->stackPointers_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      current_.reset();
    }
  }, it_);
  return *this;
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer()
{
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(Atom *symbol, int64_t offset)
{
  atomPointers_.emplace(symbol, offset);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(Extern *symbol, int64_t offset)
{
  externPointers_.emplace(symbol, offset);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(Func *func)
{
  funcPointers_.emplace(func);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(Block *block)
{
  blockPointers_.emplace(block);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(unsigned frame)
{
  stackPointers_.emplace(frame);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(unsigned frame, unsigned object, int64_t offset)
{
  framePointers_.emplace(std::make_pair(frame, object), offset);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(unsigned frame, CallSite *alloc, int64_t offset)
{
  heapPointers_.emplace(std::make_pair(frame, alloc), offset);
}

// -----------------------------------------------------------------------------
SymbolicPointer::~SymbolicPointer()
{
}

// -----------------------------------------------------------------------------
void SymbolicPointer::Add(unsigned frame, CallSite *a)
{
  heapRanges_.emplace(frame, a);
}

// -----------------------------------------------------------------------------
void SymbolicPointer::Add(unsigned frame, unsigned object)
{
  frameRanges_.emplace(frame, object);
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::Offset(int64_t adjust) const
{
  SymbolicPointer pointer;
  pointer.atomRanges_ = atomRanges_;
  pointer.frameRanges_ = frameRanges_;
  pointer.heapRanges_ = heapRanges_;
  pointer.externRanges_ = externRanges_;
  for (auto &[g, offset] : atomPointers_) {
    pointer.atomPointers_.emplace(g, offset + adjust);
  }
  for (auto &[g, offset] : framePointers_) {
    pointer.framePointers_.emplace(g, offset + adjust);
  }
  for (auto &[g, offset] : heapPointers_) {
    pointer.heapPointers_.emplace(g, offset + adjust);
  }
  for (auto &[g, offset] : externPointers_) {
    pointer.externPointers_.emplace(g, offset + adjust);
  }
  pointer.funcPointers_ = funcPointers_;
  pointer.blockPointers_ = blockPointers_;
  pointer.stackPointers_ = stackPointers_;
  return pointer;
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::Decay() const
{
  SymbolicPointer pointer;
  pointer.atomRanges_ = atomRanges_;
  pointer.frameRanges_ = frameRanges_;
  pointer.heapRanges_ = heapRanges_;
  pointer.externRanges_ = externRanges_;
  for (auto &[base, offset] : atomPointers_) {
    pointer.atomRanges_.insert(base);
  }
  for (auto &[base, offset] : framePointers_) {
    pointer.frameRanges_.insert(base);
  }
  for (auto &[base, offset] : heapPointers_) {
    pointer.heapRanges_.insert(base);
  }
  for (auto &[base, offset] : externPointers_) {
    pointer.externRanges_.insert(base);
  }
  return pointer;
}

// -----------------------------------------------------------------------------
void SymbolicPointer::LUB(const SymbolicPointer &that)
{
  for (auto range : that.atomRanges_) {
    atomRanges_.insert(range);
  }
  for (auto range : that.frameRanges_) {
    frameRanges_.insert(range);
  }
  for (auto range : that.heapRanges_) {
    heapRanges_.insert(range);
  }
  for (auto range : that.externRanges_) {
    externRanges_.insert(range);
  }
  for (auto &[g, offset] : that.atomPointers_) {
    auto it = atomPointers_.find(g);
    if (it != atomPointers_.end() && it->second != offset) {
      atomRanges_.insert(g);
    } else {
      atomPointers_.emplace(g, offset);
    }
  }
  for (auto g : atomRanges_) {
    if (auto it = atomPointers_.find(g); it != atomPointers_.end()) {
      atomPointers_.erase(it);
    }
  }
  for (auto &[g, offset] : that.framePointers_) {
    auto it = framePointers_.find(g);
    if (it != framePointers_.end() && it->second != offset) {
      frameRanges_.insert(g);
    } else {
      framePointers_.emplace(g, offset);
    }
  }
  for (auto &[g, offset] : that.heapPointers_) {
    auto it = heapPointers_.find(g);
    if (it != heapPointers_.end() && it->second != offset) {
      heapRanges_.insert(g);
    } else {
      heapPointers_.emplace(g, offset);
    }
  }
  for (auto &[g, offset] : that.externPointers_) {
    auto it = externPointers_.find(g);
    if (it != externPointers_.end() && it->second != offset) {
      externRanges_.insert(g);
    } else {
      externPointers_.emplace(g, offset);
    }
  }
  for (auto func : that.funcPointers_) {
    funcPointers_.insert(func);
  }
  for (auto block : that.blockPointers_) {
    blockPointers_.insert(block);
  }
  for (auto stack : that.stackPointers_) {
    stackPointers_.insert(stack);
  }
}

// -----------------------------------------------------------------------------
void SymbolicPointer::dump(llvm::raw_ostream &os) const
{
  bool start = true;
  unsigned i = 0;
  for (auto &address : *this) {
    if (!start) {
      os << ", ";
    }
    start = false;
    os << address;
    if (++i >= 5) {
      os << "...";
      break;
    }
  }
}

// -----------------------------------------------------------------------------
SymbolicPointer::address_iterator SymbolicPointer::begin() const
{
  if (!atomPointers_.empty()) {
    return address_iterator(atomPointers_.begin(), this);
  }
  if (!atomRanges_.empty()) {
    return address_iterator(atomRanges_.begin(), this);
  }
  if (!framePointers_.empty()) {
    return address_iterator(framePointers_.begin(), this);
  }
  if (!frameRanges_.empty()) {
    return address_iterator(frameRanges_.begin(), this);
  }
  if (!heapPointers_.empty()) {
    return address_iterator(heapPointers_.begin(), this);
  }
  if (!heapRanges_.empty()) {
    return address_iterator(heapRanges_.begin(), this);
  }
  if (!externPointers_.empty()) {
    return address_iterator(externPointers_.begin(), this);
  }
  if (!externRanges_.empty()) {
    return address_iterator(externRanges_.begin(), this);
  }
  if (!funcPointers_.empty()) {
    return address_iterator(funcPointers_.begin(), this);
  }
  if (!blockPointers_.empty()) {
    return address_iterator(blockPointers_.begin(), this);
  }
  if (!stackPointers_.empty()) {
    return address_iterator(stackPointers_.begin(), this);
  }
  return address_iterator();
}

// -----------------------------------------------------------------------------
bool SymbolicPointer::operator==(const SymbolicPointer &that) const
{
  return atomPointers_ == that.atomPointers_
      && atomRanges_ == that.atomRanges_
      && framePointers_ == that.framePointers_
      && frameRanges_ == that.frameRanges_
      && heapPointers_ == that.heapPointers_
      && heapRanges_ == that.heapRanges_
      && externPointers_ == that.externPointers_
      && externRanges_ == that.externRanges_
      && funcPointers_ == that.funcPointers_
      && blockPointers_ == that.blockPointers_;
}
