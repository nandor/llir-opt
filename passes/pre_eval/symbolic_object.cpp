// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>

#include "core/atom.h"
#include "core/data.h"
#include "core/expr.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/object.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_object.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
SymbolicObject::SymbolicObject(llvm::Align align)
  : align_(align)
{
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
  switch (type) {
    case Type::I64:
    case Type::V64: {
      if (offset % 8 != 0) {
        llvm_unreachable("not implemented");
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
          case SymbolicValue::Kind::UNKNOWN_INTEGER: {
            return (this->*mutate)(bucket, val);
          }
          // TODO
          case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
            return (this->*mutate)(bucket, SymbolicValue::UnknownInteger());
          }
          // Attempt to mix an integer into the bucket.
          case SymbolicValue::Kind::INTEGER: {
            const auto &orig = buckets_[bucket];
            switch (orig.GetKind()) {
              case SymbolicValue::Kind::UNDEFINED: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::UNKNOWN_INTEGER: {
                return (this->*mutate)(bucket, SymbolicValue::UnknownInteger());
              }
              case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::POINTER: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::VALUE: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::INTEGER: {
                APInt value = orig.GetInteger();
                value.insertBits(val.GetInteger(), bucketOffset * 8);
                return (this->*mutate)(bucket, SymbolicValue::Integer(value));
              }
            }
            llvm_unreachable("invalid bucket kind");
          }
          // TODO
          case SymbolicValue::Kind::POINTER: {
            llvm_unreachable("not implemented");
          }
          // TODO
          case SymbolicValue::Kind::VALUE: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid value kind");
      }
    }
    case Type::I128:
    case Type::F32:
    case Type::F64:
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
  switch (type) {
    case Type::I64:
    case Type::V64: {
      if (offset % 8 != 0) {
        llvm_unreachable("not implemented");
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
          case SymbolicValue::Kind::UNKNOWN_INTEGER: {
            return orig;
          }
          case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::POINTER: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::VALUE: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::INTEGER: {
            return SymbolicValue::Integer(orig.GetInteger().extractBits(
                typeSize * 8,
                bucketOffset * 8
            ));
          }
        }
        llvm_unreachable("invalid bucket kind");
      }
    }
    case Type::I128:
    case Type::F32:
    case Type::F64:
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
    buckets_[bucket] = val.LUB(buckets_[bucket]);
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
void SymbolicObject::LUB(const SymbolicObject &that)
{
  assert(buckets_.size() == that.buckets_.size() && "mismatched objects");
  for (size_t i = 0, n = buckets_.size(); i < n; ++i) {
    buckets_[i] = buckets_[i].LUB(that.buckets_[i]);
  }
}

// -----------------------------------------------------------------------------
SymbolicDataObject::SymbolicDataObject(Object &object)
  : SymbolicObject(object.begin()->GetAlignment().value_or(llvm::Align(1)))
  , object_(object)
{
  if (object.size() == 1) {
    Atom &atom = *object.begin();
    LLVM_DEBUG(llvm::dbgs() << "\nBuilding object:\n\n" << atom << "\n");
    start_.emplace(&atom, std::make_pair(0u, 0u));
    size_ = atom.GetByteSize();
    unsigned remaining = 8;
    for (auto it = atom.begin(); it != atom.end() && remaining; ) {
      Item *item = &*it++;
      switch (item->GetKind()) {
        case Item::Kind::INT8: {
          if (remaining == 8) {
            buckets_.push_back(SymbolicValue::Integer(
                llvm::APInt(64, item->GetInt8(), true)
            ));
            remaining -= 1;
            continue;
          } else {
            if (buckets_.rbegin()->IsUnknownInteger()) {
              remaining -= 1;
              continue;
            }
            llvm_unreachable("not implemented");
          }
        }
        case Item::Kind::INT16: {
          llvm_unreachable("not implemented");
        }
        case Item::Kind::INT32: {
          if (remaining == 8) {
            buckets_.push_back(SymbolicValue::Integer(
                llvm::APInt(64, item->GetInt32(), true)
            ));
            remaining -= 4;
            continue;
          } else {
            llvm_unreachable("not implemented");
          }
        }
        case Item::Kind::INT64: {
          buckets_.push_back(SymbolicValue::Integer(
              llvm::APInt(64, item->GetInt64(), true)
          ));
          continue;
        }
        case Item::Kind::EXPR: {
          auto *expr = item->GetExpr();
          switch (expr->GetKind()) {
            case Expr::Kind::SYMBOL_OFFSET: {
              auto *symExpr = static_cast<SymbolOffsetExpr *>(expr);
              buckets_.push_back(SymbolicValue::Pointer(
                  symExpr->GetSymbol(),
                  symExpr->GetOffset()
              ));
              continue;
            }
          }
          llvm_unreachable("invalid expression kind");
        }
        case Item::Kind::FLOAT64: {
          llvm_unreachable("not implemented");
        }
        case Item::Kind::SPACE: {
          unsigned n = item->GetSpace();
          unsigned i;
          for (i = 0; i + 8 <= n; i += 8) {
            buckets_.push_back(SymbolicValue::Integer(llvm::APInt(64, 0, true)));
          }
          if (i != n) {
            llvm_unreachable("not implemented");
          }
          continue;
        }
        case Item::Kind::STRING: {
          auto str = item->GetString();
          unsigned n = str.size();
          unsigned i;
          for (i = 0; i + 8 <= n; i += 8) {
            // TODO: push the actual value
            buckets_.push_back(SymbolicValue::UnknownInteger());
          }
          if (i != n) {
            buckets_.push_back(SymbolicValue::UnknownInteger());
            remaining -= n - i;
          }
          continue;
        }
      }
      llvm_unreachable("invalid item kind");
    }
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
bool SymbolicDataObject::Store(
    Atom *a,
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tStoring " << val << ":" << type << " to "
      << a->getName() << " + " << offset << "\n\n";
  );
  return WritePrecise(offset, val, type);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicDataObject::Load(Atom *a, int64_t offset, Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tLoading " << type << " from "
      << a->getName() << " + " << offset << "\n\n";
  );
  return ReadPrecise(offset, type);
}

// -----------------------------------------------------------------------------
bool SymbolicDataObject::StoreImprecise(
    Atom *a,
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  if (object_.getParent()->IsConstant()) {
    return false;
  }
  LLVM_DEBUG(llvm::dbgs()
      << "\tTainting " << type << ":" << val << " to "
      << a->getName() << " + " << offset << "\n\n";
  );
  return WriteImprecise(offset, val, type);
}

// -----------------------------------------------------------------------------
bool SymbolicDataObject::StoreImprecise(const SymbolicValue &val, Type type)
{
  if (object_.getParent()->IsConstant()) {
    return false;
  }
#ifndef NDEBUG
  LLVM_DEBUG(llvm::dbgs() << "\tTainting " << type << ":" << val << " to \n");
  for (Atom &atom : object_) {
    LLVM_DEBUG(llvm::dbgs() << "\t\t" << atom.getName() << "\n");
  }
#endif
  size_t typeSize = GetSize(type);
  bool changed = false;
  for (size_t i = 0; i + typeSize < size_; i += typeSize) {
    changed = WriteImprecise(i, val, type) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
SymbolicFrameObject::SymbolicFrameObject(
    SymbolicFrame &frame,
    unsigned object,
    size_t size,
    llvm::Align align)
  : SymbolicObject(align)
  , frame_(frame)
  , object_(object)
{
  size_ = size;
  for (unsigned i = 0, n = (size + 7) / 8; i < n; ++i) {
    buckets_.push_back(SymbolicValue::UnknownInteger());
  }
}

// -----------------------------------------------------------------------------
SymbolicFrameObject::SymbolicFrameObject(
    SymbolicFrame &frame,
    const SymbolicFrameObject &that)
  : SymbolicObject(that.align_)
  , frame_(frame)
  , object_(that.object_)
{
  size_ = that.size_;
  buckets_ = that.buckets_;
}

// -----------------------------------------------------------------------------
bool SymbolicFrameObject::Store(
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tStoring " << type << ":" << val << " to "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << " + " << offset << "\n\n";
  );
  return WritePrecise(offset, val, type);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicFrameObject::Load(int64_t offset, Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tLoading " << type << " from "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << " + " << offset << "\n\n";
  );
  return ReadPrecise(offset, type);
}

// -----------------------------------------------------------------------------
bool SymbolicFrameObject::StoreImprecise(
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tTainting " << type << ":" << val << " to "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << " + " << offset << "\n\n";
  );
  return WriteImprecise(offset, val, type);
}

// -----------------------------------------------------------------------------
bool SymbolicFrameObject::StoreImprecise(const SymbolicValue &val, Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tTainting " << type << ":" << val << " in \n"
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << "\n\n"
  );

  size_t typeSize = GetSize(type);
  bool changed = false;
  for (size_t i = 0; i + typeSize < size_; i += typeSize) {
    changed = WriteImprecise(i, val, type) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicFrameObject::LoadImprecise(Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tLoading " << type << " from "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << "\n\n";
  );

  size_t typeSize = GetSize(type);
  std::optional<SymbolicValue> value;
  for (size_t i = 0; i + typeSize < size_; i += typeSize) {
    auto v = ReadPrecise(i, type);
    if (value) {
      value = value->LUB(v);
    } else {
      value = v;
    }
  }
  assert(value && "empty frame object");
  return *value;
}

// -----------------------------------------------------------------------------
SymbolicHeapObject::SymbolicHeapObject(
    CallSite &alloc,
    std::optional<size_t> size)
  : SymbolicObject(llvm::Align(8))
  , alloc_(alloc)
  , bounded_(size)
{
  if (size) {
    size_ = *size;
    for (unsigned i = 0, n = (size_ + 7) / 8; i < n; ++i) {
      buckets_.push_back(SymbolicValue::Integer(APInt(64, 0, true)));
    }
  } else {
    size_ = 0;
  }
}

// -----------------------------------------------------------------------------
bool SymbolicHeapObject::Store(
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tStoring " << type << ":" << val << " to "
      << "<" << alloc_.getParent()->getName() << "> + " << offset << "\n\n";
  );
  if (bounded_) {
    return WritePrecise(offset, val, type);
  } else {
    return Merge(val);
  }
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicHeapObject::Load(int64_t offset, Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tLoading " << type << " from "
      << "<" << alloc_.getParent()->getName() << "> + " << offset << "\n\n";
  );
  if (bounded_) {
    return ReadPrecise(offset, type);
  } else {
    llvm_unreachable("not implemented");
  }
}

/*
// -----------------------------------------------------------------------------
bool SymbolicHeapObject::StoreImprecise(
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tTainting " << type << ":" << val << " to "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << " + " << offset << "\n\n";
  );
  if (bounded_) {
    return WriteImprecise(offset, val, type);
  } else {
    llvm_unreachable("not implemented");
  }
}
*/

// -----------------------------------------------------------------------------
bool SymbolicHeapObject::StoreImprecise(const SymbolicValue &val, Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tTaiting " << type << ":" << val << " in \n"
      << "<" << alloc_.getParent()->getName() << ">\n\n";
  );

  if (bounded_) {
    size_t typeSize = GetSize(type);
    bool changed = false;
    for (size_t i = 0; i + typeSize < size_; i += typeSize) {
      changed = WriteImprecise(i, val, type) || changed;
    }
    return changed;
  } else {
    return Merge(val);
  }
}

/*
// -----------------------------------------------------------------------------
SymbolicValue SymbolicHeapObject::LoadImprecise(Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tLoading " << type << " from "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << "\n\n";
  );

  if (bounded_) {
    size_t typeSize = GetSize(type);
    std::optional<SymbolicValue> value;
    for (size_t i = 0; i + typeSize < size_; i += typeSize) {
      auto v = ReadPrecise(i, type);
      if (value) {
        value = value->LUB(v);
      } else {
        value = v;
      }
    }
    assert(value && "empty frame object");
    return *value;
  } else {
    llvm_unreachable("not implemented");
  }
}
*/

// -----------------------------------------------------------------------------
bool SymbolicHeapObject::Merge(const SymbolicValue &val)
{
  if (approx_) {
    auto v = approx_->LUB(val);
    if (v != *approx_) {
      approx_ = v;
      return true;
    }
    return false;
  } else {
    approx_ = val;
    return true;
  }
}
