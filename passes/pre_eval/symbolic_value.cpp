// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_value.h"



// -----------------------------------------------------------------------------
SymbolicValue::SymbolicValue(const SymbolicValue &that)
  : kind_(that.kind_)
{
  switch (kind_) {
    case Kind::UNKNOWN_INTEGER:
    case Kind::UNDEFINED: {
      return;
    }
    case Kind::INTEGER: {
      new (&intVal_) APInt(that.intVal_);
      return;
    }
    case Kind::VALUE:
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
    case Kind::UNKNOWN_INTEGER:
    case Kind::UNDEFINED: {
      return *this;
    }
    case Kind::INTEGER: {
      new (&intVal_) APInt(that.intVal_);
      return *this;
    }
    case Kind::VALUE:
    case Kind::POINTER: {
      new (&ptrVal_) SymbolicPointer(that.ptrVal_);
      return *this;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::UnknownInteger()
{
  return SymbolicValue(Kind::UNKNOWN_INTEGER);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Undefined()
{
  return SymbolicValue(Kind::UNDEFINED);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Integer(const APInt &val)
{
  auto sym = SymbolicValue(Kind::INTEGER);
  new (&sym.intVal_) APInt(val);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pointer(Func *func)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_) SymbolicPointer(func);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pointer(Global *symbol, int64_t offset)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_) SymbolicPointer(symbol, offset);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pointer(unsigned frame, unsigned object, int64_t offset)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_) SymbolicPointer(frame, object, offset);
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
SymbolicValue SymbolicValue::Pointer(const SymbolicPointer &pointer)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_)SymbolicPointer(pointer);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Value(const SymbolicPointer &pointer)
{
  auto sym = SymbolicValue(Kind::VALUE);
  new (&sym.ptrVal_)SymbolicPointer(pointer);
  return sym;
}

// -----------------------------------------------------------------------------
bool SymbolicValue::IsTrue() const
{
  switch (kind_) {
    case Kind::VALUE:
    case Kind::UNKNOWN_INTEGER: {
      return false;
    }
    case Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
    case Kind::INTEGER: {
      return !intVal_.isNullValue();
    }
    case Kind::POINTER: {
      return true;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicValue::IsFalse() const
{
  switch (kind_) {
    case Kind::VALUE:
    case Kind::UNKNOWN_INTEGER: {
      return false;
    }
    case Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
    case Kind::INTEGER: {
      return intVal_.isNullValue();
    }
    case Kind::POINTER: {
      return false;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::LUB(const SymbolicValue &that) const
{
  if (*this == that) {
    return *this;
  }
  switch (kind_) {
    case Kind::UNDEFINED: {
      return *this;
    }
    case Kind::UNKNOWN_INTEGER: {
      switch (that.kind_) {
        case Kind::UNKNOWN_INTEGER:
        case Kind::UNDEFINED:
        case Kind::INTEGER: {
          return *this;
        }
        case Kind::VALUE:
        case Kind::POINTER: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::INTEGER: {
      switch (that.kind_) {
        case Kind::UNDEFINED: {
          llvm_unreachable("not implemented");
        }
        case Kind::UNKNOWN_INTEGER: {
          return SymbolicValue::UnknownInteger();
        }
        case Kind::INTEGER: {
          if (intVal_ == that.intVal_) {
            return *this;
          } else {
            return SymbolicValue::UnknownInteger();
          }
        }
        case Kind::VALUE:
        case Kind::POINTER: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::POINTER: {
      switch (that.kind_) {
        case Kind::UNDEFINED: {
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
        case Kind::VALUE: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::VALUE: {
      switch (that.kind_) {
        case Kind::UNDEFINED: {
          llvm_unreachable("not implemented");
        }
        case Kind::UNKNOWN_INTEGER: {
          llvm_unreachable("not implemented");
        }
        case Kind::INTEGER: {
          llvm_unreachable("not implemented");
        }
        case Kind::VALUE: {
          llvm_unreachable("not implemented");
        }
        case Kind::POINTER: {
          llvm_unreachable("not implemented");
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
    case Kind::UNKNOWN_INTEGER:
    case Kind::UNDEFINED: {
      return true;
    }
    case Kind::INTEGER: {
      return intVal_ == that.intVal_;
    }
    case Kind::VALUE:
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
    case Kind::UNKNOWN_INTEGER: {
      os << "unknown integer";
      return;
    }
    case Kind::UNDEFINED: {
      os << "undefined";
      return;
    }
    case Kind::INTEGER: {
      os << "int{" << intVal_ << "}";
      return;
    }
    case Kind::VALUE: {
      os << "value{" << ptrVal_ << "}";
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
    case Kind::UNKNOWN_INTEGER:
    case Kind::UNDEFINED: {
      return;
    }
    case Kind::INTEGER: {
      intVal_.~APInt();
      return;
    }
    case Kind::VALUE:
    case Kind::POINTER: {
      ptrVal_.~SymbolicPointer();
      return;
    }
  }
  llvm_unreachable("invalid kind");
}
