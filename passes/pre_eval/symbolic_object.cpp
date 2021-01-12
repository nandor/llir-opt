// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>
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
  if (offset < 0 || size_ < offset + typeSize) {
    return false;
  }

  switch (type) {
    case Type::I64:
    case Type::V64:
    case Type::F64: {
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
          case SymbolicValue::Kind::SCALAR: {
            return (this->*mutate)(bucket, val);
          }
          // TODO
          case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
            return (this->*mutate)(bucket, SymbolicValue::Scalar());
          }
          // Attempt to mix an integer into the bucket.
          case SymbolicValue::Kind::INTEGER: {
            const auto &orig = buckets_[bucket];
            switch (orig.GetKind()) {
              case SymbolicValue::Kind::UNDEFINED: {
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
  if (offset < 0 || offset + typeSize > size_) {
    return SymbolicValue::Scalar();
  }
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
          case SymbolicValue::Kind::POINTER: {
            llvm_unreachable("not implemented");
          }
          case SymbolicValue::Kind::VALUE: {
            llvm_unreachable("not implemented");
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
    for (auto it = atom.begin(); it != atom.end(); ) {
      Item *item = &*it++;
      if (remaining == 0) {
        remaining = 8;
      }
      switch (item->GetKind()) {
        case Item::Kind::INT8: {
          if (remaining == 8) {
            buckets_.push_back(SymbolicValue::Integer(
                llvm::APInt(64, item->GetInt8(), true)
            ));
            remaining -= 1;
            continue;
          } else {
            auto *last = &*buckets_.rbegin();
            if (last->IsScalar()) {
              remaining -= 1;
              continue;
            }
            if (last->IsInteger()) {
              remaining -= 1;
              if (item->GetInt8() != 0) {
                APInt value(last->GetInteger());
                value.insertBits(item->GetInt8(), remaining * 8, 8);
                *last = SymbolicValue::Integer(value);
              }
              continue;
            }
            llvm_unreachable("not implemented");
          }
        }
        case Item::Kind::INT16: {
          if (remaining == 8) {
            buckets_.push_back(SymbolicValue::Integer(
                llvm::APInt(64, item->GetInt16(), true)
            ));
            remaining -= 2;
            continue;
          } else {
            llvm_unreachable("not implemented");
          }
        }
        case Item::Kind::INT32: {
          if (remaining == 8) {
            buckets_.push_back(SymbolicValue::Integer(
                llvm::APInt(64, item->GetInt32(), true)
            ));
            remaining -= 4;
            continue;
          } else {
            auto *last = &*buckets_.rbegin();
            if (last->IsScalar()) {
              remaining -= 1;
              continue;
            }
            if (last->IsInteger()) {
              remaining -= 4;
              if (item->GetInt32() != 0) {
                APInt value(last->GetInteger());
                value.insertBits(item->GetInt32(), remaining * 8, 32);
                *last = SymbolicValue::Integer(value);
              }
              continue;
            }
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
              auto *sym = symExpr->GetSymbol();
              switch (sym->GetKind()) {
                case Global::Kind::FUNC: {
                  buckets_.push_back(SymbolicValue::Pointer(
                      &*::cast_or_null<Func>(sym)
                  ));
                  continue;
                }
                case Global::Kind::ATOM: {
                  buckets_.push_back(SymbolicValue::Pointer(
                      &*::cast_or_null<Atom>(sym), 0
                  ));
                  continue;
                }
                case Global::Kind::BLOCK: {
                  buckets_.push_back(SymbolicValue::Pointer(
                      &*::cast_or_null<Func>(sym)
                  ));
                  continue;
                }
                case Global::Kind::EXTERN: {
                  buckets_.push_back(SymbolicValue::Pointer(
                      &*::cast_or_null<Extern>(sym), 0
                  ));
                  continue;
                }
              }
              llvm_unreachable("invalid symbol kind");
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
            buckets_.push_back(SymbolicValue::Integer(llvm::APInt(64, 0, true)));
            remaining -= n - i;
          }
          continue;
        }
        case Item::Kind::STRING: {
          auto str = item->GetString();
          unsigned n = str.size();
          unsigned i;
          for (i = 0; i + 8 <= n; i += 8) {
            uint64_t bits = *reinterpret_cast<const uint64_t *>(str.data() + i);
            buckets_.push_back(SymbolicValue::Integer(llvm::APInt(64, bits, true)));
          }
          if (i != n) {
            uint64_t bits = 0;
            for (unsigned j = 0; j < 8; ++j) {
              unsigned idx = 7 - j;
              uint64_t byte = i + idx < n ? str[i + idx] : 0;
              bits = (bits << 8ull) | byte;
            }
            buckets_.push_back(SymbolicValue::Integer(llvm::APInt(64, bits, true)));
            remaining -= n - i;
          }
          continue;
        }
      }
      llvm_unreachable("invalid item kind");
    }
    for (unsigned i = 0, n = buckets_.size(); i < n; ++i) {
      LLVM_DEBUG(llvm::dbgs() << "\t" << i << ": " << buckets_[i] << '\n');
    }
    assert(size_ <= buckets_.size() * 8 && "invalid object");
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
SymbolicDataObject::SymbolicDataObject(const SymbolicDataObject &that)
  : SymbolicObject(that.align_)
  , object_(that.object_)
  , start_(that.start_)
{
  size_ = that.size_;
  buckets_ = that.buckets_;
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
  for (size_t i = 0; i + typeSize <= size_; i += typeSize) {
    changed = WriteImprecise(i, val, type) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicDataObject::LoadImprecise(Type type)
{
  #ifndef NDEBUG
  LLVM_DEBUG(llvm::dbgs() << "\tLoading " << type << " from \n");
   for (Atom &atom : object_) {
    LLVM_DEBUG(llvm::dbgs() << "\t\t" << atom.getName() << "\n");
  }
  #endif

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
  return value ? *value : SymbolicValue::Scalar();
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
    buckets_.push_back(SymbolicValue::Scalar());
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
std::pair<unsigned, unsigned> SymbolicFrameObject::GetID()
{
  return { frame_.GetIndex(), object_};
}

// -----------------------------------------------------------------------------
bool SymbolicFrameObject::Store(
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tStoring " << type << ":" << val << " to frame "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << ", " << this << " + " << offset << "\n\n";
  );
  return WritePrecise(offset, val, type);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicFrameObject::Load(int64_t offset, Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tLoading " << type << " from frame "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << ", " << this << " + " << offset << "\n\n";
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
      << "\tTainting " << type << ":" << val << " in frame "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << " + " << offset << "\n\n";
  );
  return WriteImprecise(offset, val, type);
}

// -----------------------------------------------------------------------------
bool SymbolicFrameObject::StoreImprecise(const SymbolicValue &val, Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tTainting " << type << ":" << val << " in frame "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << "\n\n"
  );

  size_t typeSize = GetSize(type);
  bool changed = false;
  for (size_t i = 0; i + typeSize <= size_; i += typeSize) {
    changed = WriteImprecise(i, val, type) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicFrameObject::LoadImprecise(Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tLoading " << type << " from frame "
      << (frame_.GetFunc() ? frame_.GetFunc()->getName() : "argv")
      << ":" << object_ << "\n\n";
  );

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
  return value ? *value : SymbolicValue::Scalar();
}

// -----------------------------------------------------------------------------
SymbolicHeapObject::SymbolicHeapObject(
    unsigned frame,
    CallSite &alloc,
    std::optional<size_t> size)
  : SymbolicObject(llvm::Align(8))
  , frame_(frame)
  , alloc_(alloc)
  , bounded_(size && *size < 256 * 8)
{
  if (size && *size < 256 * 8) {
    size_ = *size;
    for (unsigned i = 0, n = (size_ + 7) / 8; i < n; ++i) {
      buckets_.push_back(SymbolicValue::Integer(APInt(64, 0, true)));
    }
  } else {
    size_ = 0;
    buckets_.push_back(SymbolicValue::Scalar());
  }
}

// -----------------------------------------------------------------------------
SymbolicHeapObject::SymbolicHeapObject(const SymbolicHeapObject &that)
  : SymbolicObject(that.align_)
  , alloc_(that.alloc_)
  , bounded_(that.bounded_)
{
  size_ = that.size_;
  buckets_ = that.buckets_;
}

// -----------------------------------------------------------------------------
bool SymbolicHeapObject::Store(
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tStoring " << type << ":" << val << " to heap "
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
      << "\tLoading " << type << " from heap "
      << "<" << alloc_.getParent()->getName() << "> + " << offset << "\n\n";
  );
  if (bounded_) {
    return ReadPrecise(offset, type);
  } else {
    return buckets_[0];
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
      << "\tTaiting "
      << "<" << alloc_.getParent()->getName() << "> with "
      << type << ":" << val << "\n\n";
  );

  if (bounded_) {
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
SymbolicValue SymbolicHeapObject::LoadImprecise(Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "\tLoading " << type << " from "
      << "<" << alloc_.getParent()->getName() << ">\n\n";
  );

  if (bounded_) {
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
    return value ? *value : SymbolicValue::Scalar();
  } else {
    assert(buckets_.size() == 1 && "missing approximate value");
    return buckets_[0];
  }
}

// -----------------------------------------------------------------------------
bool SymbolicHeapObject::Merge(const SymbolicValue &val)
{
  auto v = buckets_[0].LUB(val);
  if (v != buckets_[0]) {
    buckets_[0] = v;
    return true;
  }
  return false;
}
