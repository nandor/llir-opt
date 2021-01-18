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
    case Kind::OBJECT: {
      return v_.O.Object == that.v_.O.Object && v_.O.Offset == that.v_.O.Offset;
    }
    case Kind::OBJECT_RANGE: {
      return v_.OR.Object == that.v_.OR.Object;
    }
    case Kind::EXTERN: {
      return v_.E.Symbol == that.v_.E.Symbol && v_.E.Offset == that.v_.E.Offset;
    }
    case Kind::EXTERN_RANGE: {
      return v_.ER.Symbol == that.v_.ER.Symbol;
    }
    case Kind::FUNC: {
      return v_.F.F == that.v_.F.F;
    }
    case Kind::BLOCK: {
      return v_.B.B == that.v_.B.B;
    }
    case Kind::STACK: {
      return v_.S.Frame == that.v_.S.Frame;
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
void SymbolicAddress::dump(llvm::raw_ostream &os) const
{
  switch (v_.K) {
    case Kind::OBJECT: {
      os << "<" << v_.O.Object << "> + " << v_.O.Offset;
      return;
    }
    case Kind::OBJECT_RANGE: {
      os << "<" << v_.OR.Object << ">";
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
      os << v_.F.F->getName();
      return;
    }
    case Kind::BLOCK: {
      os << v_.B.B->getName();
      return;
    }
    case Kind::STACK: {
      os << "[" << v_.S.Frame << "]";
      return;
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
bool SymbolicAddress::IsPrecise() const
{
  switch (GetKind()) {
    case Kind::OBJECT:
    case Kind::EXTERN:
    case Kind::FUNC:
    case Kind::BLOCK:
    case Kind::STACK: {
      return true;
    }
    case Kind::OBJECT_RANGE:
    case Kind::EXTERN_RANGE: {
      return false;
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// -----------------------------------------------------------------------------
SymbolicPointer::address_iterator &
SymbolicPointer::address_iterator::operator++()
{
  std::visit(overloaded {
    [this] (std::pair<ObjectMap::const_iterator, BitSet<SymbolicObject>::iterator> it) {
      auto [ot, ft] = it;
      if (++ft != ot->second.end()) {
        it_ = std::make_pair(ot, ft);
        current_.emplace(std::make_pair(ot, ft));
        return;
      }
      if (++ot != pointer_->objectPointers_.end()) {
        it_ = std::make_pair(ot, ot->second.begin());
        current_.emplace(std::make_pair(ot, ot->second.begin()));
        return;
      }
      if (!pointer_->objectRanges_.Empty()) {
        it_ = pointer_->objectRanges_.begin();
        current_.emplace(pointer_->objectRanges_.begin());
        return;
      }
      if (!pointer_->externPointers_.empty()) {
        it_ = pointer_->externPointers_.begin();
        current_.emplace(pointer_->externPointers_.begin());
        return;
      }
      if (!pointer_->externRanges_.empty()) {
        it_ = pointer_->externRanges_.begin();
        current_.emplace(pointer_->externRanges_.begin());
        return;
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (ObjectRangeMap::iterator it) {
      if (++it != pointer_->objectRanges_.end()) {
        it_ = it;
        current_.emplace(it);
        return;
      }
      if (!pointer_->externPointers_.empty()) {
        it_ = pointer_->externPointers_.begin();
        current_.emplace(pointer_->externPointers_.begin());
        return;
      }
      if (!pointer_->externRanges_.empty()) {
        it_ = pointer_->externRanges_.begin();
        current_.emplace(pointer_->externRanges_.begin());
        return;
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (ExternMap::const_iterator it) {
      if (++it != pointer_->externPointers_.end()) {
        it_ = it;
        current_.emplace(it);
        return;
      }
      if (!pointer_->externRanges_.empty()) {
        it_ = pointer_->externRanges_.begin();
        current_.emplace(pointer_->externRanges_.begin());
        return;
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (ExternRangeMap::const_iterator it) {
      if (++it != pointer_->externRanges_.end()) {
        it_ = it;
        current_.emplace(it);
        return;
      }
      if (!pointer_->funcPointers_.empty()) {
        it_ = pointer_->funcPointers_.begin();
        current_.emplace(pointer_->funcPointers_.begin());
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (FuncMap::const_iterator it) {
      if (++it != pointer_->funcPointers_.end()) {
        it_ = it;
        current_.emplace(it);
        return;
      }
      if (!pointer_->blockPointers_.empty()) {
        it_ = pointer_->blockPointers_.begin();
        current_.emplace(pointer_->blockPointers_.begin());
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (BlockMap::const_iterator it) {
      if (++it != pointer_->blockPointers_.end()) {
        it_ = it;
        current_.emplace(it);
        return;
      }
      if (!pointer_->stackPointers_.empty()) {
        it_ = pointer_->stackPointers_.begin();
        current_.emplace(pointer_->stackPointers_.begin());
        return;
      }
      current_.reset();
    },
    [this] (StackMap::const_iterator it) {
      if (++it != pointer_->stackPointers_.end()) {
        it_ = it;
        current_.emplace(it);
        return;
      }
      current_.reset();
    }
  }, it_);
  return *this;
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer()
{
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(ID<SymbolicObject> object, int64_t offset)
{
  objectPointers_[offset].Insert(object);
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
SymbolicPointer::~SymbolicPointer()
{
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::Offset(int64_t adjust) const
{
  SymbolicPointer pointer;
  for (auto &[offset, pointers] : objectPointers_) {
    pointer.objectPointers_.emplace(offset + adjust, pointers);
  }
  pointer.objectRanges_ = objectRanges_;
  for (auto &[g, offset] : externPointers_) {
    pointer.externPointers_.emplace(g, offset + adjust);
  }
  pointer.externRanges_ = externRanges_;
  return pointer;
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::Decay() const
{
  SymbolicPointer pointer;
  pointer.objectRanges_ = objectRanges_;
  pointer.externRanges_ = externRanges_;
  for (auto &[offset, pointers] : objectPointers_) {
    pointer.objectRanges_.Union(pointers);
  }
  for (auto &[base, offset] : externPointers_) {
    pointer.externRanges_.insert(base);
  }
  return pointer;
}

// -----------------------------------------------------------------------------
void SymbolicPointer::LUB(const SymbolicPointer &that)
{
  // Find the set of all symbols that are in both objects.
  BitSet<SymbolicObject> inThis(objectRanges_);
  for (auto &[offset, mask] : objectPointers_) {
    inThis |= mask;
  }
  BitSet<SymbolicObject> inThat(that.objectRanges_);
  for (auto &[offset, mask] : objectPointers_) {
    inThis |= mask;
  }
  BitSet<SymbolicObject> in(inThis | inThat);

  auto &thisObjs = objectPointers_;
  auto &thatObjs = that.objectPointers_;
  auto thisIt = thisObjs.begin();
  auto thatIt = thatObjs.begin();
  ObjectMap objectPointers;
  while (thisIt != thisObjs.end() && thatIt != thatObjs.end()) {
    if (thatIt != thatObjs.end()) {
      while (thisIt != thisObjs.end() && thisIt->first < thatIt->first) {
        auto diff = thisIt->second - inThat;
        if (!diff.Empty()) {
          objectPointers.emplace(thisIt->first, std::move(diff));
        }
        ++thisIt;
      }
    }
    if (thisIt != thisObjs.end()) {
      while (thatIt != thatObjs.end() && thatIt->first < thisIt->first) {
        auto diff = thatIt->second - inThis;
        if (!diff.Empty()) {
          objectPointers.emplace(thatIt->first, std::move(diff));
        }
        ++thatIt;
      }
    }
    if (thisIt != thisObjs.end() && thatIt != thatObjs.end()) {
      if (thisIt->first == thatIt->first) {
        auto &mthis = thisIt->second;
        auto &mthat = thatIt->second;
        objectPointers.emplace(
            thisIt->first,
            (mthis & mthat) | (mthis - inThat) | (mthat - inThis)
        );
        ++thisIt;
        ++thatIt;
      }
    }
  }
  while (thisIt != thisObjs.end()) {
    auto diff = thisIt->second - inThat;
    if (!diff.Empty()) {
      objectPointers.emplace(thisIt->first, std::move(diff));
    }
    ++thisIt;
  }
  while (thatIt != thatObjs.end()) {
    auto diff = thatIt->second - inThis;
    if (!diff.Empty()) {
      objectPointers.emplace(thatIt->first, std::move(diff));
    }
    ++thatIt;
  }

  for (auto [offset, mask] : objectPointers) {
    in.Subtract(mask);
  }
  std::swap(objectPointers_, objectPointers);
  std::swap(objectRanges_, in);

  // Build the LUB of other pointers.
  for (auto &[g, offset] : that.externPointers_) {
    auto it = externPointers_.find(g);
    if (it != externPointers_.end() && it->second != offset) {
      externRanges_.insert(g);
    } else {
      externPointers_.emplace(g, offset);
    }
  }
  for (auto range : that.externRanges_) {
    externRanges_.insert(range);
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
    /*
    if (++i >= 5) {
      os << "...";
      break;
    }
    */
  }
}

// -----------------------------------------------------------------------------
SymbolicPointer::address_iterator SymbolicPointer::begin() const
{
  if (!objectPointers_.empty()) {
    auto begin = objectPointers_.begin();
    return address_iterator(std::make_pair(begin, begin->second.begin()), this);
  }
  if (!objectRanges_.Empty()) {
    return address_iterator(objectRanges_.begin(), this);
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
  return objectPointers_ == that.objectPointers_
      && objectRanges_ == that.objectRanges_
      && externPointers_ == that.externPointers_
      && externRanges_ == that.externRanges_
      && funcPointers_ == that.funcPointers_
      && blockPointers_ == that.blockPointers_
      && stackPointers_ == that.stackPointers_;
}
