// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/global.h"
#include "core/func.h"
#include "core/insts.h"
#include "passes/pre_eval/symbolic_pointer.h"



// -----------------------------------------------------------------------------
bool SymbolicAddress::operator==(const SymbolicAddress &that) const
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicAddress::dump(llvm::raw_ostream &os) const
{
  switch (v_.K) {
    case Kind::GLOBAL: {
      os << v_.G.Symbol->getName() << " + " << v_.G.Offset;
      return;
    }
    case Kind::GLOBAL_RANGE: {
      os << v_.GR.Symbol->getName();
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
    case Kind::FUNC: {
      os << v_.Fn.Fn->getName();
      return;
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
    [this] (GlobalMap::const_iterator it) {
      if (++it != pointer_->globalPointers_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->globalRanges_.empty()) {
        it_ = pointer_->globalRanges_.begin();
        current_.emplace(*pointer_->globalRanges_.begin());
        return;
      }
      if (!pointer_->framePointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->frameRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->heapPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->heapRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->funcPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      current_.reset();
    },
    [this] (GlobalRangeMap::const_iterator it) {
      if (++it != pointer_->globalRanges_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->framePointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->frameRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->heapPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->heapRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->funcPointers_.empty()) {
        llvm_unreachable("not implemented");
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
        llvm_unreachable("not implemented");
      }
      if (!pointer_->heapPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->heapRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->funcPointers_.empty()) {
        llvm_unreachable("not implemented");
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
        llvm_unreachable("not implemented");
      }
      if (!pointer_->heapRanges_.empty()) {
        llvm_unreachable("not implemented");
      }
      if (!pointer_->funcPointers_.empty()) {
        llvm_unreachable("not implemented");
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
        llvm_unreachable("not implemented");
      }
      current_.reset();
    },
    [this] (HeapRangeMap::const_iterator it) {
      if (++it != pointer_->heapRanges_.end()) {
        it_ = it;
        current_.emplace(*it);
        return;
      }
      if (!pointer_->funcPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      current_.reset();
    },
    [this] (FuncMap::const_iterator it) {
      if (++it != pointer_->funcPointers_.end()) {
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
SymbolicPointer::SymbolicPointer(Global *symbol, int64_t offset)
{
  globalPointers_.emplace(symbol, offset);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(Func *func)
{
  funcPointers_.emplace(func);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(unsigned frame, unsigned object, int64_t offset)
{
  framePointers_.emplace(std::make_pair(frame, object), offset);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(CallSite *alloc, int64_t offset)
{
  heapPointers_.emplace(alloc, offset);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(const SymbolicPointer &that)
  : globalPointers_(that.globalPointers_)
  , globalRanges_(that.globalRanges_)
  , framePointers_(that.framePointers_)
  , frameRanges_(that.frameRanges_)
  , heapPointers_(that.heapPointers_)
  , heapRanges_(that.heapRanges_)
  , funcPointers_(that.funcPointers_)
{
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(SymbolicPointer &&that)
  : globalPointers_(std::move(that.globalPointers_))
  , globalRanges_(std::move(that.globalRanges_))
  , framePointers_(std::move(that.framePointers_))
  , frameRanges_(std::move(that.frameRanges_))
  , heapPointers_(std::move(that.heapPointers_))
  , heapRanges_(std::move(that.heapRanges_))
  , funcPointers_(std::move(that.funcPointers_))
{
}

// -----------------------------------------------------------------------------
SymbolicPointer::~SymbolicPointer()
{
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::Offset(int64_t adjust) const
{
  SymbolicPointer pointer;
  pointer.globalRanges_ = globalRanges_;
  pointer.frameRanges_ = frameRanges_;
  pointer.heapRanges_ = heapRanges_;
  for (auto &[g, offset] : globalPointers_) {
    pointer.globalPointers_.emplace(g, offset + adjust);
  }
  for (auto &[g, offset] : framePointers_) {
    pointer.framePointers_.emplace(g, offset + adjust);
  }
  for (auto &[g, offset] : heapPointers_) {
    pointer.heapPointers_.emplace(g, offset + adjust);
  }
  pointer.funcPointers_ = funcPointers_;
  return pointer;
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::Decay() const
{
  SymbolicPointer pointer;
  pointer.globalRanges_ = globalRanges_;
  pointer.frameRanges_ = frameRanges_;
  pointer.heapRanges_ = heapRanges_;
  for (auto &[base, offset] : globalPointers_) {
    pointer.globalRanges_.insert(base);
  }
  for (auto &[base, offset] : framePointers_) {
    pointer.frameRanges_.insert(base);
  }
  for (auto &[base, offset] : heapPointers_) {
    pointer.heapRanges_.insert(base);
  }
  pointer.funcPointers_ = funcPointers_;
  return pointer;
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::LUB(const SymbolicPointer &that) const
{
  SymbolicPointer pointer;
  pointer.globalRanges_ = globalRanges_;
  for (auto range : that.globalRanges_) {
    pointer.globalRanges_.insert(range);
  }
  for (auto range : that.frameRanges_) {
    pointer.frameRanges_.insert(range);
  }
  for (auto range : that.heapRanges_) {
    pointer.heapRanges_.insert(range);
  }
  for (auto &[g, offset] : that.globalPointers_) {
    auto it = globalPointers_.find(g);
    if (it != globalPointers_.end() && it->second != offset) {
      pointer.globalRanges_.insert(g);
    } else {
      pointer.globalPointers_.emplace(g, offset);
    }
  }
  for (auto &[g, offset] : that.framePointers_) {
    auto it = framePointers_.find(g);
    if (it != framePointers_.end() && it->second != offset) {
      pointer.frameRanges_.insert(g);
    } else {
      pointer.framePointers_.emplace(g, offset);
    }
  }
  for (auto &[g, offset] : that.heapPointers_) {
    auto it = heapPointers_.find(g);
    if (it != heapPointers_.end() && it->second != offset) {
      pointer.heapRanges_.insert(g);
    } else {
      pointer.heapPointers_.emplace(g, offset);
    }
  }
  for (auto func : that.funcPointers_) {
    pointer.funcPointers_.insert(func);
  }
  return pointer;
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
  if (!globalPointers_.empty()) {
    return address_iterator(globalPointers_.begin(), this);
  }
  if (!globalRanges_.empty()) {
    return address_iterator(globalRanges_.begin(), this);
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
  if (!funcPointers_.empty()) {
    return address_iterator(funcPointers_.begin(), this);
  }
  return address_iterator();
}

// -----------------------------------------------------------------------------
bool SymbolicPointer::operator==(const SymbolicPointer &that) const
{
  return globalPointers_ == that.globalPointers_
      && globalRanges_ == that.globalRanges_
      && framePointers_ == that.framePointers_
      && frameRanges_ == that.frameRanges_
      && heapPointers_ == that.heapPointers_
      && heapRanges_ == that.heapRanges_
      && funcPointers_ == that.funcPointers_;
}
