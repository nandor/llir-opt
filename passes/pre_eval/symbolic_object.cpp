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
  , size_(size)
  , align_(align)
  , rdonly_(rdonly)
{
  if (size_) {
    if (zero) {
      new (&v_.B) BucketStorage(*size_, SymbolicValue::Integer(APInt(64, 0, true)));
    } else {
      new (&v_.B) BucketStorage(*size_, SymbolicValue::Scalar());
    }
  } else {
    if (zero) {
      new (&v_.M) MergedStorage(SymbolicValue::Integer(APInt(64, 0, true)));
    } else {
      new (&v_.M) MergedStorage(SymbolicValue::Scalar());
    }
  }
}

// -----------------------------------------------------------------------------
SymbolicObject::SymbolicObject(const SymbolicObject &that)
  : id_(that.id_)
  , size_(that.size_)
  , align_(that.align_)
  , rdonly_(that.rdonly_)
{
  if (that.v_.Accurate) {
    new (&v_.B) BucketStorage(that.v_.B);
  } else {
    new (&v_.M) MergedStorage(that.v_.M);
  }
}

// -----------------------------------------------------------------------------
SymbolicObject::~SymbolicObject()
{
  if (v_.Accurate) {
    v_.B.~BucketStorage();
  } else {
    v_.M.~MergedStorage();
  }
}

// -----------------------------------------------------------------------------
const SymbolicValue *SymbolicObject::begin() const
{
  if (v_.Accurate) {
    return v_.B.begin();
  } else {
    return v_.M.begin();
  }
}

// -----------------------------------------------------------------------------
const SymbolicValue *SymbolicObject::end() const
{
  if (v_.Accurate) {
    return v_.B.begin();
  } else {
    return v_.M.begin();
  }
}

// -----------------------------------------------------------------------------
void SymbolicObject::Merge(const SymbolicObject &that)
{
  assert(size_ == that.size_ && "mismatched size");
  assert(align_ == that.align_ && "mismatched alignment");
  assert(rdonly_ == that.rdonly_ && "mismatched flags");

  if (v_.Accurate) {
    if (that.v_.Accurate) {
      v_.B.Merge(that.v_.B);
    } else {
      const auto &value = v_.B.Load();
      v_.B.~BucketStorage();
      new (&v_.M) MergedStorage(value);
      v_.M.Store(that.v_.M.Load());
    }
  } else {
    if (that.v_.Accurate) {
      v_.M.Store(that.v_.B.Load());
    } else {
      v_.M.Store(that.v_.M.Load());
    }
  }
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicObject::Load(int64_t offset, Type type)
{
  if (v_.Accurate) {
    return v_.B.Load(offset, type);
  } else {
    return Cast(v_.M.Load(), type);
  }
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicObject::LoadImprecise(Type type)
{
  if (v_.Accurate) {
    return Cast(v_.B.Load(), type);
  } else {
    return Cast(v_.M.Load(), type);
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::Init(int64_t offset, const SymbolicValue &val, Type type)
{
  if (v_.Accurate) {
    return v_.B.StorePrecise(offset, val, type);
  } else {
    return v_.M.Store(val);
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::Store(int64_t offset, const SymbolicValue &val, Type type)
{
  if (rdonly_) {
    return false;
  }

  if (v_.Accurate) {
    return v_.B.StorePrecise(offset, val, type);
  } else {
    if (size_) {
      const auto &value = v_.M.Load();
      v_.M.~MergedStorage();
      new (&v_.B) BucketStorage(*size_, value);
      return v_.B.StorePrecise(offset, val, type);
    } else {
      return v_.M.Store(val);
    }
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

  if (v_.Accurate) {
    return v_.B.StoreImprecise(offset, val, type);
  } else {
    return v_.M.Store(val);
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::StoreImprecise(const SymbolicValue &val, Type type)
{
  if (rdonly_) {
    return false;
  }

  if (v_.Accurate) {
    const auto &value = v_.B.Load();
    v_.B.~BucketStorage();
    new (&v_.M) MergedStorage(value);
    return v_.M.Store(val);
  } else {
    return v_.M.Store(val);
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::MergedStorage::Store(const SymbolicValue &value)
{
  if (value_ != value) {
    value_.Merge(value);
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
SymbolicObject::BucketStorage::BucketStorage(
    size_t size,
    const SymbolicValue &value)
{
  for (unsigned i = 0, n = Clamp(size + 7) / 8; i < n; ++i) {
    buckets_.push_back(value);
  }
}

// -----------------------------------------------------------------------------
void SymbolicObject::BucketStorage::Merge(const BucketStorage &that)
{
  assert(buckets_.size() == that.buckets_.size());
  for (unsigned i = 0, n = buckets_.size(); i < n; ++i) {
    buckets_[i].Merge(that.buckets_[i]);
  }
  approx_.Merge(that.approx_);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicObject::BucketStorage::Load(
    int64_t offset,
    Type type) const
{
  const unsigned bucket = offset / 8;
  if (bucket < buckets_.size()) {
    return Read(offset, type);
  } else {
    return Cast(approx_, type);
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::BucketStorage::StorePrecise(
    int64_t offset,
    const SymbolicValue &value,
    Type type)
{
  const unsigned bucket = offset / 8;
  if (bucket < buckets_.size()) {
    if (Write(offset, value, type, &BucketStorage::Set)) {
      approx_.Merge(value);
      return true;
    }
    return false;
  } else {
    if (approx_ != value) {
      approx_.Merge(value);
      return true;
    }
    return false;
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::BucketStorage::StoreImprecise(
    int64_t offset,
    const SymbolicValue &value,
    Type type)
{
  const unsigned bucket = offset / 8;
  if (bucket < buckets_.size()) {
    if (Write(offset, value, type, &BucketStorage::Merge)) {
      approx_.Merge(value);
      return true;
    }
    return false;
  } else {
    if (approx_ != value) {
      approx_.Merge(value);
      return true;
    }
    return false;
  }
}

// -----------------------------------------------------------------------------
bool SymbolicObject::BucketStorage::Write(
    int64_t offset,
    const SymbolicValue &val,
    Type type,
    bool (BucketStorage::*mutate)(unsigned, const SymbolicValue &))
{
  // This only works for single-atom objects.
  unsigned bucket = offset / 8;
  unsigned bucketOffset = offset - bucket * 8;
  size_t typeSize = GetSize(type);
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
SymbolicValue SymbolicObject::BucketStorage::Read(
    int64_t offset,
    Type type) const
{
  // This only works for single-atom objects.
  unsigned bucket = offset / 8;
  unsigned bucketOffset = offset - bucket * 8;
  size_t typeSize = GetSize(type);

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
bool SymbolicObject::BucketStorage::Set(
    unsigned bucket,
    const SymbolicValue &val)
{
  if (val != buckets_[bucket]) {
    buckets_[bucket] = val;
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicObject::BucketStorage::Merge(
    unsigned bucket,
    const SymbolicValue &val)
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
