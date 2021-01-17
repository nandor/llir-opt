// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_value.h"



// -----------------------------------------------------------------------------
SymbolicValue::SymbolicValue(const SymbolicValue &that)
  : kind_(that.kind_)
{
  switch (kind_) {
    case Kind::SCALAR:
    case Kind::UNDEFINED: {
      return;
    }
    case Kind::LOWER_BOUNDED_INTEGER:
    case Kind::INTEGER: {
      new (&intVal_) APInt(that.intVal_);
      return;
    }
    case Kind::FLOAT: {
      new (&floatVal_) APFloat(that.floatVal_);
      return;
    }
    case Kind::VALUE:
    case Kind::POINTER:
    case Kind::NULLABLE: {
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
    case Kind::SCALAR:
    case Kind::UNDEFINED: {
      return *this;
    }
    case Kind::LOWER_BOUNDED_INTEGER:
    case Kind::INTEGER: {
      new (&intVal_) APInt(that.intVal_);
      return *this;
    }
    case Kind::FLOAT: {
      new (&floatVal_) APFloat(that.floatVal_);
      return *this;
    }
    case Kind::VALUE:
    case Kind::POINTER:
    case Kind::NULLABLE: {
      new (&ptrVal_) SymbolicPointer(that.ptrVal_);
      return *this;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Scalar()
{
  return SymbolicValue(Kind::SCALAR);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Undefined()
{
  return SymbolicValue(Kind::UNDEFINED);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Float(const APFloat &val)
{
  auto sym = SymbolicValue(Kind::FLOAT);
  new (&sym.floatVal_) APFloat(val);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Integer(const APInt &val)
{
  auto sym = SymbolicValue(Kind::INTEGER);
  new (&sym.intVal_) APInt(val);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::LowerBoundedInteger(const APInt &val)
{
  auto sym = SymbolicValue(Kind::LOWER_BOUNDED_INTEGER);
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
SymbolicValue SymbolicValue::Pointer(Block *block)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_) SymbolicPointer(block);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pointer(ID<SymbolicObject> object, int64_t offset)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_) SymbolicPointer(object, offset);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pointer(Extern *symbol, int64_t offset)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_) SymbolicPointer(symbol, offset);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pointer(SymbolicPointer &&pointer)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_) SymbolicPointer(std::move(pointer));
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pointer(const SymbolicPointer &pointer)
{
  auto sym = SymbolicValue(Kind::POINTER);
  new (&sym.ptrVal_) SymbolicPointer(pointer);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Value(const SymbolicPointer &pointer)
{
  auto sym = SymbolicValue(Kind::VALUE);
  new (&sym.ptrVal_) SymbolicPointer(pointer);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Nullable(const SymbolicPointer &pointer)
{
  auto sym = SymbolicValue(Kind::NULLABLE);
  new (&sym.ptrVal_) SymbolicPointer(pointer);
  return sym;
}

// -----------------------------------------------------------------------------
bool SymbolicValue::IsTrue() const
{
  switch (kind_) {
    case Kind::VALUE:
    case Kind::SCALAR:
    case Kind::NULLABLE: {
      return false;
    }
    case Kind::LOWER_BOUNDED_INTEGER: {
      return !intVal_.isNullValue();
    }
    case Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
    case Kind::FLOAT: {
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
    case Kind::SCALAR:
    case Kind::LOWER_BOUNDED_INTEGER:
    case Kind::NULLABLE: {
      return false;
    }
    case Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
    case Kind::FLOAT: {
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
      return that;
    }
    case Kind::LOWER_BOUNDED_INTEGER: {
      switch (that.kind_) {
        case Kind::SCALAR: {
          return SymbolicValue::Scalar();
        }
        case Kind::LOWER_BOUNDED_INTEGER: {
          return SymbolicValue::LowerBoundedInteger(llvm::APIntOps::umin(
              intVal_,
              that.intVal_
          ));
        }
        case Kind::UNDEFINED: {
          llvm_unreachable("not implemented");
        }
        case Kind::INTEGER: {
          if (that.intVal_.isNonNegative()) {
            return SymbolicValue::LowerBoundedInteger(llvm::APIntOps::umin(
                intVal_,
                that.intVal_
            ));
          } else {
            return SymbolicValue::Scalar();
          }
        }
        case Kind::VALUE:
        case Kind::NULLABLE:
        case Kind::POINTER: {
          return SymbolicValue::Value(that.ptrVal_);
        }
        case Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::SCALAR: {
      switch (that.kind_) {
        case Kind::SCALAR:
        case Kind::UNDEFINED:
        case Kind::INTEGER: {
          return *this;
        }
        case Kind::FLOAT:
        case Kind::LOWER_BOUNDED_INTEGER: {
          return SymbolicValue::Scalar();
        }
        case Kind::VALUE:
        case Kind::POINTER:
        case Kind::NULLABLE: {
          return SymbolicValue::Value(that.ptrVal_);
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::INTEGER: {
      switch (that.kind_) {
        case Kind::UNDEFINED: {
          llvm_unreachable("not implemented");
        }
        case Kind::LOWER_BOUNDED_INTEGER: {
          if (intVal_.isNonNegative()) {
            return SymbolicValue::LowerBoundedInteger(llvm::APIntOps::umin(
                intVal_,
                that.intVal_
            ));
          } else {
            return SymbolicValue::Scalar();
          }
        }
        case Kind::SCALAR: {
          return SymbolicValue::Scalar();
        }
        case Kind::INTEGER: {
          if (intVal_ == that.intVal_) {
            return *this;
          } else if (intVal_.isNonNegative() && that.intVal_.isNonNegative()) {
            return SymbolicValue::LowerBoundedInteger(llvm::APIntOps::umin(
                intVal_,
                that.intVal_
            ));
          } else {
            return SymbolicValue::Scalar();
          }
        }
        case Kind::POINTER: {
          if (intVal_.isNullValue()) {
            return SymbolicValue::Nullable(that.ptrVal_);
          } else {
            return SymbolicValue::Value(that.ptrVal_);
          }
        }
        case Kind::VALUE:
        case Kind::NULLABLE: {
          return SymbolicValue::Value(that.ptrVal_);
        }
        case Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::POINTER: {
      switch (that.kind_) {
        case Kind::UNDEFINED: {
          return *this;
        }
        case Kind::SCALAR:
        case Kind::LOWER_BOUNDED_INTEGER: {
          return SymbolicValue::Value(ptrVal_);
        }
        case Kind::INTEGER: {
          if (that.intVal_.isNullValue()) {
            return SymbolicValue::Nullable(ptrVal_);
          } else {
            return SymbolicValue::Value(ptrVal_);
          }
        }
        case Kind::POINTER: {
          SymbolicPointer ptr(ptrVal_);
          ptr.LUB(that.ptrVal_);
          return SymbolicValue::Pointer(ptr);
        }
        case Kind::VALUE: {
          SymbolicPointer ptr(ptrVal_);
          ptr.LUB(that.ptrVal_);
          return SymbolicValue::Value(ptr);
        }
        case Kind::NULLABLE: {
          SymbolicPointer ptr(ptrVal_);
          ptr.LUB(that.ptrVal_);
          return SymbolicValue::Nullable(ptr);
        }
        case Kind::FLOAT: {
          return SymbolicValue::Value(ptrVal_);
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::VALUE: {
      switch (that.kind_) {
        case Kind::UNDEFINED: {
          return *this;
        }
        case Kind::SCALAR:
        case Kind::LOWER_BOUNDED_INTEGER: {
          return SymbolicValue::Value(ptrVal_);
        }
        case Kind::INTEGER: {
          return *this;
        }
        case Kind::VALUE:
        case Kind::POINTER:
        case Kind::NULLABLE:  {
          SymbolicPointer ptr(ptrVal_);
          ptr.LUB(that.ptrVal_);
          return SymbolicValue::Value(ptr);
        }
        case Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::NULLABLE: {
      switch (that.kind_) {
        case Kind::UNDEFINED: {
          llvm_unreachable("not implemented");
        }
        case Kind::SCALAR:
        case Kind::LOWER_BOUNDED_INTEGER: {
          return SymbolicValue::Value(ptrVal_);
        }
        case Kind::INTEGER: {
          if (that.intVal_.isNullValue()) {
            return *this;
          } else {
            return SymbolicValue::Value(ptrVal_);
          }
        }
        case Kind::NULLABLE:
        case Kind::POINTER: {
          SymbolicPointer ptr(ptrVal_);
          ptr.LUB(that.ptrVal_);
          return SymbolicValue::Nullable(ptr);
        }
        case Kind::VALUE: {
          SymbolicPointer ptr(ptrVal_);
          ptr.LUB(that.ptrVal_);
          return SymbolicValue::Value(ptr);
        }
        case Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::FLOAT: {
      switch (that.kind_) {
        case Kind::UNDEFINED: {
          llvm_unreachable("not implemented");
        }
        case Kind::SCALAR: {
          return that;
        }
        case Kind::LOWER_BOUNDED_INTEGER: {
          llvm_unreachable("not implemented");
        }
        case Kind::INTEGER: {
          llvm_unreachable("not implemented");
        }
        case Kind::NULLABLE: {
          llvm_unreachable("not implemented");
        }
        case Kind::POINTER: {
          llvm_unreachable("not implemented");
        }
        case Kind::VALUE: {
          llvm_unreachable("not implemented");
        }
        case Kind::FLOAT: {
          if (floatVal_ == that.floatVal_) {
            return *this;
          } else {
            return SymbolicValue::Scalar();
          }
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
    case Kind::SCALAR:
    case Kind::UNDEFINED: {
      return true;
    }
    case Kind::LOWER_BOUNDED_INTEGER:
    case Kind::INTEGER: {
      return intVal_ == that.intVal_;
    }
    case Kind::FLOAT: {
      return floatVal_ == that.floatVal_;
    }
    case Kind::VALUE:
    case Kind::POINTER:
    case Kind::NULLABLE: {
      return ptrVal_ == that.ptrVal_;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void SymbolicValue::dump(llvm::raw_ostream &os) const
{
  switch (kind_) {
    case Kind::SCALAR: {
      os << "scalar";
      return;
    }
    case Kind::UNDEFINED: {
      os << "undefined";
      return;
    }
    case Kind::LOWER_BOUNDED_INTEGER: {
      os << "bound{" << intVal_ << " <= *}";
      return;
    }
    case Kind::INTEGER: {
      os << "int{" << intVal_ << "}";
      return;
    }
    case Kind::FLOAT: {
      llvm::SmallVector<char, 16> buffer;
      floatVal_.toString(buffer);
      os << "float{" << buffer << "}";
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
    case Kind::NULLABLE: {
      os << "nullable{" << ptrVal_ << "}";
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void SymbolicValue::Destroy()
{
  switch (kind_) {
    case Kind::SCALAR:
    case Kind::UNDEFINED:
    case Kind::LOWER_BOUNDED_INTEGER: {
      return;
    }
    case Kind::INTEGER: {
      intVal_.~APInt();
      return;
    }
    case Kind::FLOAT: {
      floatVal_.~APFloat();
      return;
    }
    case Kind::VALUE:
    case Kind::POINTER:
    case Kind::NULLABLE: {
      ptrVal_.~SymbolicPointer();
      return;
    }
  }
  llvm_unreachable("invalid kind");
}
