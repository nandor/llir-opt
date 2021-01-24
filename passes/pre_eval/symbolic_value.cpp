// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_value.h"



// -----------------------------------------------------------------------------
SymbolicValue::SymbolicValue(const SymbolicValue &that)
  : kind_(that.kind_)
  , origin_(that.origin_)
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
    case Kind::MASKED_INTEGER: {
      new (&maskVal_.Known) APInt(that.maskVal_.Known);
      new (&maskVal_.Value) APInt(that.maskVal_.Value);
      return;
    }
    case Kind::FLOAT: {
      new (&floatVal_) APFloat(that.floatVal_);
      return;
    }
    case Kind::VALUE:
    case Kind::POINTER:
    case Kind::NULLABLE: {
      new (&ptrVal_) std::shared_ptr(that.ptrVal_);
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
  origin_ = that.origin_;

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
    case Kind::MASKED_INTEGER: {
      llvm_unreachable("not implemented");
    }
    case Kind::FLOAT: {
      new (&floatVal_) APFloat(that.floatVal_);
      return *this;
    }
    case Kind::VALUE:
    case Kind::POINTER:
    case Kind::NULLABLE: {
      new (&ptrVal_) std::shared_ptr(that.ptrVal_);
      return *this;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Scalar(const std::optional<Origin> &orig)
{
  return SymbolicValue(Kind::SCALAR, orig);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Undefined(const std::optional<Origin> &orig)
{
  return SymbolicValue(Kind::UNDEFINED, orig);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Float(
    const APFloat &val,
    const std::optional<Origin> &orig)
{
  auto sym = SymbolicValue(Kind::FLOAT, orig);
  new (&sym.floatVal_) APFloat(val);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Integer(
    const APInt &val,
    const std::optional<Origin> &orig)
{
  auto sym = SymbolicValue(Kind::INTEGER, orig);
  new (&sym.intVal_) APInt(val);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::LowerBoundedInteger(
    const APInt &val,
    const std::optional<Origin> &orig)
{
  auto sym = SymbolicValue(Kind::LOWER_BOUNDED_INTEGER, orig);
  new (&sym.intVal_) APInt(val);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Mask(
    const APInt &known,
    const APInt &value,
    const std::optional<Origin> &orig)
{
  auto sym = SymbolicValue(Kind::MASKED_INTEGER, orig);
  new (&sym.maskVal_.Known) APInt(known);
  new (&sym.maskVal_.Value) APInt(value);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pointer(
    const std::shared_ptr<SymbolicPointer> &pointer,
    const std::optional<Origin> &orig)
{
  auto sym = SymbolicValue(Kind::POINTER, orig);
  new (&sym.ptrVal_) std::shared_ptr(pointer);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Value(
    const std::shared_ptr<SymbolicPointer> &pointer,
    const std::optional<Origin> &orig)
{
  auto sym = SymbolicValue(Kind::VALUE, orig);
  new (&sym.ptrVal_) std::shared_ptr(pointer);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Nullable(
    const std::shared_ptr<SymbolicPointer> &pointer,
    const std::optional<Origin> &orig)
{
  auto sym = SymbolicValue(Kind::NULLABLE, orig);
  new (&sym.ptrVal_) std::shared_ptr(pointer);
  return sym;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicValue::Pin(Ref<Inst> ref, ID<SymbolicFrame> frame) const
{
  SymbolicValue that(*this);
  that.origin_.emplace(frame, ref);
  return that;
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
      return intVal_[0];
    }
    case Kind::MASKED_INTEGER: {
      return (maskVal_.Value & maskVal_.Known)[0];
    }
    case Kind::UNDEFINED: {
      return false;
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
    case Kind::MASKED_INTEGER: {
      return maskVal_.Known[0] && !maskVal_.Value[0];
    }
    case Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
    case Kind::FLOAT: {
      llvm_unreachable("not implemented");
    }
    case Kind::INTEGER: {
      return !intVal_[0];
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
        case Kind::MASKED_INTEGER: {
          llvm_unreachable("not implemented");
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
          return SymbolicValue::Scalar();
        }
        case Kind::FLOAT:
        case Kind::LOWER_BOUNDED_INTEGER: {
          return SymbolicValue::Scalar();
        }
        case Kind::MASKED_INTEGER: {
          llvm_unreachable("not implemented");
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
        case Kind::MASKED_INTEGER: {
          llvm_unreachable("not implemented");
        }
        case Kind::SCALAR: {
          return SymbolicValue::Scalar();
        }
        case Kind::INTEGER: {
          if (intVal_ == that.intVal_) {
            return SymbolicValue::Integer(intVal_);
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
          return SymbolicValue::Pointer(ptrVal_);
        }
        case Kind::SCALAR:
        case Kind::LOWER_BOUNDED_INTEGER: {
          return SymbolicValue::Value(ptrVal_);
        }
        case Kind::MASKED_INTEGER: {
          llvm_unreachable("not implemented");
        }
        case Kind::INTEGER: {
          if (that.intVal_.isNullValue()) {
            return SymbolicValue::Nullable(ptrVal_);
          } else {
            return SymbolicValue::Value(ptrVal_);
          }
        }
        case Kind::POINTER: {
          return SymbolicValue::Pointer(ptrVal_->LUB(that.ptrVal_));
        }
        case Kind::VALUE: {
          return SymbolicValue::Value(ptrVal_->LUB(that.ptrVal_));
        }
        case Kind::NULLABLE: {
          return SymbolicValue::Nullable(ptrVal_->LUB(that.ptrVal_));
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
          return SymbolicValue::Value(ptrVal_);
        }
        case Kind::FLOAT:
        case Kind::SCALAR:
        case Kind::LOWER_BOUNDED_INTEGER:
        case Kind::MASKED_INTEGER:
        case Kind::INTEGER: {
          return SymbolicValue::Value(ptrVal_);
        }
        case Kind::VALUE:
        case Kind::POINTER:
        case Kind::NULLABLE:  {
          return SymbolicValue::Value(ptrVal_->LUB(that.ptrVal_));
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
        case Kind::MASKED_INTEGER: {
          llvm_unreachable("not implemented");
        }
        case Kind::INTEGER: {
          if (that.intVal_.isNullValue()) {
            return SymbolicValue::Nullable(ptrVal_);
          } else {
            return SymbolicValue::Value(ptrVal_);
          }
        }
        case Kind::NULLABLE:
        case Kind::POINTER: {
          return SymbolicValue::Nullable(ptrVal_->LUB(that.ptrVal_));
        }
        case Kind::VALUE: {
          return SymbolicValue::Value(ptrVal_->LUB(that.ptrVal_));
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
          return SymbolicValue::Scalar();
        }
        case Kind::LOWER_BOUNDED_INTEGER: {
          llvm_unreachable("not implemented");
        }
        case Kind::MASKED_INTEGER: {
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
          return SymbolicValue::Scalar();
        }
        case Kind::FLOAT: {
          if (floatVal_ == that.floatVal_) {
            return SymbolicValue::Float(floatVal_);
          } else {
            return SymbolicValue::Scalar();
          }
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Kind::MASKED_INTEGER: {
      llvm_unreachable("not implemented");
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
  if (origin_ != that.origin_) {
    return false;
  }
  switch (kind_) {
    case Kind::SCALAR:
    case Kind::UNDEFINED: {
      return true;
    }
    case Kind::MASKED_INTEGER: {
      return maskVal_.Known == that.maskVal_.Known
          && maskVal_.Value == that.maskVal_.Value;
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
  if (origin_) {
    llvm::errs() << origin_->second.Get() << "@" << origin_->first << ":";
  }
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
    case Kind::MASKED_INTEGER: {
      os << "mask{" << maskVal_.Known << ", " << maskVal_.Value << "}";
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
      os << "value{" << *ptrVal_ << "}";
      return;
    }
    case Kind::POINTER: {
      os << "pointer{" << *ptrVal_ << "}";
      return;
    }
    case Kind::NULLABLE: {
      os << "nullable{" << *ptrVal_ << "}";
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
    case Kind::MASKED_INTEGER: {
      maskVal_.Known.~APInt();
      maskVal_.Value.~APInt();
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
      ptrVal_.~shared_ptr();
      return;
    }
  }
  llvm_unreachable("invalid kind");
}
