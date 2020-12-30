// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/global.h"
#include "core/func.h"
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
        current_.emplace(it->first, it->second);
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
      if (!pointer_->funcPointers_.empty()) {
        llvm_unreachable("not implemented");
      }
      current_.reset();
    },
    [this] (FrameMap::const_iterator it) {
      if (++it != pointer_->framePointers_.end()) {
        it_ = it;
        current_.emplace(it->first.first, it->first.second, it->second);
        return;
      }
      if (!pointer_->frameRanges_.empty()) {
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
        current_.emplace(it->first, it->second);
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
SymbolicPointer::SymbolicPointer(const SymbolicPointer &that)
  : globalPointers_(that.globalPointers_)
  , globalRanges_(that.globalRanges_)
  , framePointers_(that.framePointers_)
  , frameRanges_(that.frameRanges_)
  , funcPointers_(that.funcPointers_)
{
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(SymbolicPointer &&that)
  : globalPointers_(std::move(that.globalPointers_))
  , globalRanges_(std::move(that.globalRanges_))
  , framePointers_(std::move(that.framePointers_))
  , frameRanges_(std::move(that.frameRanges_))
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
  for (auto &[g, offset] : globalPointers_) {
    pointer.globalPointers_.emplace(g, offset + adjust);
  }
  for (auto &[g, offset] : framePointers_) {
    pointer.framePointers_.emplace(g, offset + adjust);
  }
  return pointer;
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::Decay() const
{
  SymbolicPointer pointer;
  pointer.globalRanges_ = globalRanges_;
  pointer.frameRanges_ = frameRanges_;
  for (auto &[base, offset] : globalPointers_) {
    pointer.globalRanges_.insert(base);
  }
  for (auto &[base, offset] : framePointers_) {
    pointer.frameRanges_.insert(base);
  }
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
  return pointer;
}

// -----------------------------------------------------------------------------
void SymbolicPointer::dump(llvm::raw_ostream &os) const
{
  bool start = true;
  unsigned i = 0;
  for (auto &address : addresses()) {
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
SymbolicPointer::address_iterator SymbolicPointer::address_begin() const
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
      && funcPointers_ == that.funcPointers_;
}
