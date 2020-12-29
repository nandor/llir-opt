// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/global.h"
#include "core/adt/hash.h"
#include "passes/pre_eval/symbolic_value.h"



// -----------------------------------------------------------------------------
size_t SymbolicAddress::AddrGlobalHash::operator()(const AddrGlobal &that) const
{
  size_t hash = 0;
  hash_combine(hash, std::hash<uint8_t>{}(static_cast<uint8_t>(that.K)));
  hash_combine(hash, std::hash<Global *>{}(that.Symbol));
  hash_combine(hash, std::hash<int64_t>{}(that.Offset));
  return hash;
}

// -----------------------------------------------------------------------------
size_t SymbolicAddress::Hash::operator()(const SymbolicAddress &that) const
{
  switch (that.v_.K) {
    case SymbolicAddress::Kind::GLOBAL: return AddrGlobalHash{}(that.v_.G);
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
bool SymbolicAddress::S::operator==(const SymbolicAddress::S &that) const
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
std::optional<std::pair<Global *, int64_t>> SymbolicAddress::ToGlobal() const
{
  switch (v_.K) {
    case Kind::GLOBAL: {
      return std::make_pair(v_.G.Symbol, v_.G.Offset);
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
bool SymbolicAddress::operator==(const SymbolicAddress &that) const
{
  return v_ == that.v_;
}

// -----------------------------------------------------------------------------
void SymbolicAddress::dump(llvm::raw_ostream &os) const
{
  switch (v_.K) {
    case Kind::GLOBAL: {
      os << v_.G.Symbol->getName() << "+" << v_.G.Offset;
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(Global *symbol, int64_t offset)
{
  addresses_.emplace(symbol, offset);
}

// -----------------------------------------------------------------------------
SymbolicPointer::SymbolicPointer(const SymbolicPointer &that)
  : addresses_(that.addresses_)
{
}

// -----------------------------------------------------------------------------
SymbolicPointer::~SymbolicPointer()
{
}

// -----------------------------------------------------------------------------
void SymbolicPointer::dump(llvm::raw_ostream &os) const
{
  bool start = true;
  for (auto &addr : addresses_) {
    if (!start) {
      os << ", ";
    }
    start = false;
    os << addr;
  }
}

// -----------------------------------------------------------------------------
std::optional<std::pair<Global *, int64_t>> SymbolicPointer::ToPrecise() const
{
  if (addresses_.size() == 1) {
    return addresses_.begin()->ToGlobal();
  } else {
    llvm_unreachable("TODO: not precise");
  }
}

// -----------------------------------------------------------------------------
SymbolicValue::SymbolicValue(const SymbolicValue &that)
  : kind_(that.kind_)
{
  switch (kind_) {
    case Kind::UNKNOWN: {
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
    case Kind::UNKNOWN: {
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
SymbolicValue SymbolicValue::Integer(const APInt &val)
{
  auto sym = SymbolicValue(Kind::UNKNOWN);
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
bool SymbolicValue::operator==(const SymbolicValue &that) const
{
  if (kind_ != that.kind_) {
    return false;
  }
  switch (kind_) {
    case Kind::UNKNOWN: {
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
void SymbolicValue::dump(llvm::raw_ostream &os) const
{
  switch (kind_) {
    case Kind::UNKNOWN: {
      os << "unknown";
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
    case Kind::UNKNOWN: {
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
