// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/global.h"
#include "core/adt/hash.h"
#include "passes/pre_eval/symbolic_value.h"



// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer()
{
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(Global *symbol, int64_t offset)
{
  pointers_.emplace(symbol, offset);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(const SymbolicPointer &that)
  : pointers_(that.pointers_)
  , ranges_(that.ranges_)
{
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(SymbolicPointer &&that)
  : pointers_(std::move(that.pointers_))
  , ranges_(std::move(that.ranges_))
{
}

// -----------------------------------------------------------------------------
SymbolicPointer::~SymbolicPointer()
{
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::LUB(const SymbolicPointer &that) const
{
  SymbolicPointer pointer;
  pointer.ranges_ = ranges_;
  for (Global *range : that.ranges_) {
    pointer.ranges_.insert(range);
  }
  for (auto &[g, offset] : that.pointers_) {
    auto it = pointers_.find(g);
    if (it != pointers_.end() && it->second != offset) {
      pointer.ranges_.insert(g);
    } else {
      pointer.pointers_.emplace(g, offset);
    }
  }
  return pointer;
}

// -----------------------------------------------------------------------------
void SymbolicPointer::dump(llvm::raw_ostream &os) const
{
  bool start = true;
  for (auto *g : ranges_) {
    if (!start) {
      os << ", ";
    }
    start = false;
    os << g->getName();
  }
  for (auto &[g, offset] : pointers_) {
    if (!start) {
      os << ", ";
    }
    start = false;
    os << g->getName() << "+" << offset;
  }
}

// -----------------------------------------------------------------------------
bool SymbolicPointer::operator==(const SymbolicPointer &that) const
{
  return pointers_ == that.pointers_ && ranges_ == that.ranges_;
}

// -----------------------------------------------------------------------------
std::optional<std::pair<Global *, int64_t>> SymbolicPointer::ToPrecise() const
{
  if (ranges_.empty() && pointers_.size() == 1) {
    auto &[g, offset] = *pointers_.begin();
    return std::make_pair(g, offset);
  } else {
    return std::nullopt;
  }
}

// -----------------------------------------------------------------------------
SymbolicValue::SymbolicValue(const SymbolicValue &that)
  : kind_(that.kind_)
{
  switch (kind_) {
    case Kind::UNKNOWN:
    case Kind::UNKNOWN_INTEGER: {
      return;
    }
    case Kind::INTEGER: {
      new (&intVal_) APInt(that.intVal_);
      return;
    }
    case Kind::POINTER: {
      new (&ptrVal_) SymbolicPointer(that.ptrVal_);
      return;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
SymbolicValue::~SymbolicValue()
{
  Destroy();
}

// -----------------------------------------------------------------------------
SymbolicValue &SymbolicValue::operator=(const SymbolicValue &that)
{
  Destroy();

  kind_ = that.kind_;

  switch (kind_) {
    case Kind::UNKNOWN:
    case Kind::UNKNOWN_INTEGER: {
      return *this;
    }
    case Kind::INTEGER: {
      new (&intVal_) APInt(that.intVal_);
      return *this;
    }
    case Kind::POINTER: {
      new (&ptrVal_) SymbolicPointer(that.ptrVal_);
      return *this;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Unknown()
{
  return SymbolicValue(Kind::UNKNOWN);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::UnknownInteger()
{
  return SymbolicValue(Kind::UNKNOWN_INTEGER);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Integer(const APInt &val)
{
  auto sym = SymbolicValue(Kind::INTEGER);
  new (&sym.intVal_) APInt(val);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Address(Global *symbol, int64_t offset)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_) SymbolicPointer(symbol, offset);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pointer(SymbolicPointer &&pointer)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_)SymbolicPointer(std::move(pointer));
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::LUB(const SymbolicValue &that) const
{
  if (*this == that) {
    return *this;
  }
  switch (kind_) {
    case Kind::UNKNOWN: {
      return *this;
    }
    case Kind::UNKNOWN_INTEGER: {
      switch (that.kind_) {
        case Kind::UNKNOWN:
        case Kind::UNKNOWN_INTEGER:
        case Kind::INTEGER: {
          return *this;
        }
        case Kind::POINTER: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::INTEGER: {
      switch (that.kind_) {
        case Kind::UNKNOWN:{
          llvm_unreachable("not implemented");
        }
        case Kind::UNKNOWN_INTEGER:{
          return SymbolicValue::UnknownInteger();
        }
        case Kind::INTEGER: {
          if (intVal_ == that.intVal_) {
            return *this;
          } else {
            return SymbolicValue::UnknownInteger();
          }
        }
        case Kind::POINTER: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::POINTER: {
      switch (that.kind_) {
        case Kind::UNKNOWN:{
          llvm_unreachable("not implemented");
        }
        case Kind::UNKNOWN_INTEGER:{
          llvm_unreachable("not implemented");
        }
        case Kind::INTEGER: {
          llvm_unreachable("not implemented");
        }
        case Kind::POINTER: {
          return SymbolicValue::Pointer(ptrVal_.LUB(that.ptrVal_));
        }
      }
      llvm_unreachable("invalid value kind");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicValue::operator==(const SymbolicValue &that) const
{
  if (kind_ != that.kind_) {
    return false;
  }
  switch (kind_) {
    case Kind::UNKNOWN:
    case Kind::UNKNOWN_INTEGER: {
      return true;
    }
    case Kind::INTEGER: {
      return intVal_ == that.intVal_;
    }
    case Kind::POINTER: {
      return ptrVal_ == that.ptrVal_;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicPointer::Offset(int64_t adjust) const
{
  SymbolicPointer pointer;
  pointer.ranges_ = ranges_;
  for (auto &[g, offset] : pointers_) {
    pointer.pointers_.emplace(g, offset + adjust);
  }
  return pointer;
}

// -----------------------------------------------------------------------------
void SymbolicValue::dump(llvm::raw_ostream &os) const
{
  switch (kind_) {
    case Kind::UNKNOWN: {
      os << "unknown";
      return;
    }
    case Kind::UNKNOWN_INTEGER: {
      os << "unknown integer";
      return;
    }
    case Kind::INTEGER: {
      os << "int{" << intVal_ << "}";
      return;
    }
    case Kind::POINTER: {
      os << "pointer{" << ptrVal_ << "}";
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void SymbolicValue::Destroy()
{
  switch (kind_) {
    case Kind::UNKNOWN:
    case Kind::UNKNOWN_INTEGER: {
      return;
    }
    case Kind::INTEGER: {
      intVal_.~APInt();
      return;
    }
    case Kind::POINTER: {
      ptrVal_.~SymbolicPointer();
      return;
    }
  }
  llvm_unreachable("invalid kind");
}
