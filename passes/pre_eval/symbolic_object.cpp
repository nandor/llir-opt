// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Format.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/data.h"
#include "core/expr.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/object.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_object.h"



// -----------------------------------------------------------------------------
SymbolicValue Cast(const SymbolicValue &value, Type type)
{
  switch (value.GetKind()) {
    case SymbolicValue::Kind::UNDEFINED:
    case SymbolicValue::Kind::SCALAR: {
      return value;
    }
    case SymbolicValue::Kind::INTEGER: {
      const auto &i = value.GetInteger();
      switch (type) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::V64: {
          return SymbolicValue::Integer(i.zextOrTrunc(GetBitWidth(type)));
        }
        case Type::I128: {
          llvm_unreachable("not implemented");
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          return SymbolicValue::Scalar();
        }
      }
      llvm_unreachable("invalid type");
    }
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      return SymbolicValue::Scalar();
    }
    case SymbolicValue::Kind::MASKED_INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::FLOAT: {
      switch (type) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::V64: {
          return SymbolicValue::Scalar();
        }
        case Type::I128: {
          llvm_unreachable("not implemented");
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid type");
    }
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::NULLABLE:
    case SymbolicValue::Kind::VALUE: {
      switch (type) {
        case Type::I8:
        case Type::I16:
        case Type::I32: {
          return SymbolicValue::Scalar();
        }
        case Type::I64:
        case Type::V64: {
          return value;
        }
        case Type::I128: {
          llvm_unreachable("not implemented");
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          return SymbolicValue::Scalar();
        }
      }
      llvm_unreachable("invalid type");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
static size_t Clamp(size_t size)
{
  return std::min<size_t>(256, ((size + 7) / 8) * 8);
}

// -----------------------------------------------------------------------------
SymbolicObject::SymbolicObject(
    ID<SymbolicObject> id,
    std::optional<size_t> size,
    llvm::Align align,
    bool rdonly,
    bool zero)
  : id_(id)
  , size_(size ? std::optional(Clamp(*size)) : std::nullopt)
  , align_(align)
  , rdonly_(rdonly)
{
  if (zero) {
    for (unsigned i = 0, n = size_ ? (*size_ + 7) / 8 : 1; i < n; ++i) {
      buckets_.push_back(SymbolicValue::Integer(APInt(64, 0, true)));
    }
  } else {
    for (unsigned i = 0, n = size_ ? (*size_ + 7) / 8 : 1; i < n; ++i) {
      buckets_.push_back(SymbolicValue::Scalar());
    }
  }
}

// -----------------------------------------------------------------------------
SymbolicObject::~SymbolicObject()
{
}

// -----------------------------------------------------------------------------
bool SymbolicObject::WritePrecise(
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  return Write(offset, val, type, &SymbolicObject::Set);
}

// -----------------------------------------------------------------------------
bool SymbolicObject::WriteImprecise(
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  return Write(offset, val, type, &SymbolicObject::Merge);
}

// -----------------------------------------------------------------------------
bool SymbolicObject::Write(
    int64_t offset,
    const SymbolicValue &val,
    Type type,
    bool (SymbolicObject::*mutate)(unsigned, const SymbolicValue &))
{
  // This only works for single-atom objects.
  unsigned bucket = offset / 8;
  unsigned bucketOffset = offset - bucket * 8;
  size_t typeSize = GetSize(type);
  assert(size_ && 0 <= offset && offset + typeSize <= *size_);
  switch (type) {
    case Type::I64:
    case Type::V64:
    case Type::F64: {
      if (offset % 8 != 0) {
        return false;
      } else {
        return (this->*mutate)(bucket, val);
      }
    }
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      if  (bucketOffset + typeSize > 8) {
        llvm_unreachable("not implemented");
      } else {
        switch (val.GetKind()) {
          // Propagate undefined.
          case SymbolicValue::Kind::UNDEFINED: {
            llvm_unreachable("not implemented");
          }
          // If the incoming value is unknown, invalidate the whole bucket.
          case SymbolicValue::Kind::SCALAR: {
            return (this->*mutate)(bucket, val);
          }
          // TODO
          case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
            return (this->*mutate)(bucket, SymbolicValue::Scalar());
          }
          case SymbolicValue::Kind::MASKED_INTEGER: {
            const auto &orig = buckets_[bucket];
            switch (orig.GetKind()) {
              case SymbolicValue::Kind::UNDEFINED: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::MASKED_INTEGER: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::SCALAR: {
                return (this->*mutate)(bucket, SymbolicValue::Scalar());
              }
              case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::POINTER:
              case SymbolicValue::Kind::VALUE:
              case SymbolicValue::Kind::NULLABLE: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::INTEGER: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::FLOAT: {
                llvm_unreachable("not implemented");
              }
            }
            llvm_unreachable("not implemented");
          }
          // Attempt to mix an integer into the bucket.
          case SymbolicValue::Kind::INTEGER: {
            const auto &orig = buckets_[bucket];
            switch (orig.GetKind()) {
              case SymbolicValue::Kind::UNDEFINED: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::MASKED_INTEGER: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::SCALAR:
              case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
                return (this->*mutate)(bucket, SymbolicValue::Scalar());
              }
              case SymbolicValue::Kind::POINTER:
              case SymbolicValue::Kind::VALUE:
              case SymbolicValue::Kind::NULLABLE: {
                return (this->*mutate)(bucket, SymbolicValue::Scalar());
              }
              case SymbolicValue::Kind::INTEGER: {
                APInt value = orig.GetInteger();
                value.insertBits(val.GetInteger(), bucketOffset * 8);
                return (this->*mutate)(bucket, SymbolicValue::Integer(value));
              }
              case SymbolicValue::Kind::FLOAT: {
                llvm_unreachable("not implemented");
              }
            }
            llvm_unreachable("invalid bucket kind");
          }
          // TODO
          case SymbolicValue::Kind::FLOAT: {
            llvm_unreachable("not implemented");
          }
          // TODO
          case SymbolicValue::Kind::POINTER:
          case SymbolicValue::Kind::VALUE:
          case SymbolicValue::Kind::NULLABLE: {
            return (this->*mutate)(bucket, val.LUB(buckets_[bucket]));
          }
        }
        llvm_unreachable("invalid value kind");
      }
    }
    case Type::I128:
    case Type::F32:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicObject::ReadPrecise(int64_t offset, Type type)
{
  // This only works for single-atom objects.
  unsigned bucket = offset / 8;
  unsigned bucketOffset = offset - bucket * 8;
  size_t typeSize = GetSize(type);
  assert(size_ && 0 <= offset && offset + typeSize <= *size_);

  switch (type) {
    case Type::I64:
    case Type::V64: {
      if (offset % 8 != 0) {
        return SymbolicValue::Scalar();
      } else {
        return buckets_[bucket];
      }
    }
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      if  (bucketOffset + typeSize > 8) {
        llvm_unreachable("not implemented");
      } else {
        const auto &orig = buckets_[bucket];
        switch (orig.GetKind()) {
          case SymbolicValue::Kind::UNDEFINED:
          case SymbolicValue::Kind::SCALAR: {
            return orig;
          }
          case SymbolicValue::Kind::POINTER:
          case SymbolicValue::Kind::VALUE:
          case SymbolicValue::Kind::NULLABLE: {
            return SymbolicValue::Scalar();
          }
          case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
            return SymbolicValue::Scalar();
          }
          case SymbolicValue::Kind::MASKED_INTEGER: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::INTEGER: {
            return SymbolicValue::Integer(orig.GetInteger().extractBits(
                typeSize * 8,
                bucketOffset * 8
            ));
          }
          // TODO
          case SymbolicValue::Kind::FLOAT: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid bucket kind");
      }
    }
    case Type::F64: {
      if (offset % 8 != 0) {
        llvm_unreachable("not implemented");
      } else {
        const auto &orig = buckets_[bucket];
        switch (orig.GetKind()) {
          case SymbolicValue::Kind::UNDEFINED: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::SCALAR: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::MASKED_INTEGER: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::POINTER: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::VALUE: {
            return SymbolicValue::Scalar();
          }
          case SymbolicValue::Kind::NULLABLE: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::INTEGER: {
            return SymbolicValue::Float(APFloat(
                APFloat::IEEEdouble(),
                orig.GetInteger()
            ));
          }
          case SymbolicValue::Kind::FLOAT: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("not implemented");
      }
    }
    case Type::I128:
    case Type::F32:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
bool SymbolicObject::Set(unsigned bucket, const SymbolicValue &val)
{
  if (val != buckets_[bucket]) {
    buckets_[bucket] = val;
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicObject::Merge(unsigned bucket, const SymbolicValue &val)
{
  if (val != buckets_[bucket]) {
    auto lub = val.LUB(buckets_[bucket]);
    if (lub != buckets_[bucket]) {
      buckets_[bucket] = lub;
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
void SymbolicObject::LUB(const SymbolicObject &that)
{
  assert(align_ == that.align_ && "mismatched alignment");
  assert(buckets_.size() == that.buckets_.size() && "mismatched buckets");
  assert(size_ == that.size_ && "mismatched size");

  for (size_t i = 0, n = buckets_.size(); i < n; ++i) {
    buckets_[i] = buckets_[i].LUB(that.buckets_[i]);
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::Init(int64_t offset, const SymbolicValue &val, Type type)
{
  if (size_) {
    unsigned bucket = offset / 8;
    if (bucket >= buckets_.size()) {
      return Merge(buckets_.size() - 1, Cast(val, Type::I64));
    } else {
      return WritePrecise(offset, val, type);
    }
  } else {
    return Merge(Cast(val, Type::I64));
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::Store(int64_t offset, const SymbolicValue &val, Type type)
{
  return !rdonly_ && Init(offset, val, type);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicObject::Load(
    int64_t offset,
    Type type)
{
  if (size_) {
    unsigned bucket = offset / 8;
    if (bucket >= buckets_.size()) {
      return Cast(buckets_[buckets_.size() - 1], type);
    } else {
      return ReadPrecise(offset, type);
    }
  } else {
    return Cast(buckets_[0], type);
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::StoreImprecise(
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  if (rdonly_) {
    return false;
  }

  if (size_) {
    unsigned bucket = offset / 8;
    if (bucket >= buckets_.size()) {
      return Merge(buckets_.size() - 1, Cast(val, Type::I64));
    } else {
      return WriteImprecise(offset, val, type);
    }
  } else {
    return Merge(Cast(val, Type::I64));
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::StoreImprecise(const SymbolicValue &val, Type type)
{
  if (size_) {
    size_t typeSize = GetSize(type);
    bool changed = false;
    for (size_t i = 0; i + typeSize <= size_; i += typeSize) {
      changed = WriteImprecise(i, val, type) || changed;
    }
    return changed;
  } else {
    return Merge(val);
  }
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicObject::LoadImprecise(Type type)
{
  if (size_) {
    size_t typeSize = GetSize(type);
    std::optional<SymbolicValue> value;
    for (size_t i = 0; i + typeSize <= size_; i += typeSize) {
      auto v = ReadPrecise(i, type);
      if (value) {
        value = value->LUB(v);
      } else {
        value = v;
      }
    }
    return value ? Cast(*value, type) : SymbolicValue::Scalar();
  } else {
    return Cast(buckets_[0], type);
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::Merge(const SymbolicValue &val)
{
  auto v = buckets_[0].LUB(val);
  if (v != buckets_[0]) {
    buckets_[0] = v;
    return true;
  }
  return false;
}
