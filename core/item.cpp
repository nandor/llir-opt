// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/item.h"
#include "core/atom.h"
#include "core/expr.h"
#include "core/cast.h"



// -----------------------------------------------------------------------------
Item::Item(Item &that)
  : kind_(that.GetKind())
  , parent_(nullptr)
{
  switch (that.GetKind()) {
    case Item::Kind::INT8: {
      int8val_ = that.int8val_;
      return;
    }
    case Item::Kind::INT16: {
      int16val_ = that.int16val_;
      return;
    }
    case Item::Kind::INT32: {
      int32val_ = that.int32val_;
      return;
    }
    case Item::Kind::INT64: {
      int64val_ = that.int64val_;
      return;
    }
    case Item::Kind::FLOAT64: {
      float64val_ = that.float64val_;
      return;
    }
    case Item::Kind::SPACE: {
      int32val_ = that.int32val_;
      return;
    }
    case Item::Kind::EXPR32:
    case Item::Kind::EXPR64: {
      new (&useVal_) Use(that.useVal_.get(), nullptr);
      return;
    }
    case Item::Kind::STRING: {
      new (&stringVal_) std::string(that.stringVal_);
      return;
    }
  }
  llvm_unreachable("invalid item kind");
}

// -----------------------------------------------------------------------------
Item *Item::CreateInt8(int8_t val)
{
  Item *item = new Item(Kind::INT8);
  item->int8val_ = val;
  return item;
}

// -----------------------------------------------------------------------------
Item *Item::CreateInt16(int16_t val)
{
  Item *item = new Item(Kind::INT16);
  item->int16val_ = val;
  return item;
}

// -----------------------------------------------------------------------------
Item *Item::CreateInt32(int32_t val)
{
  Item *item = new Item(Kind::INT32);
  item->int32val_ = val;
  return item;
}

// -----------------------------------------------------------------------------
Item *Item::CreateInt64(int64_t val)
{
  Item *item = new Item(Kind::INT64);
  item->int64val_ = val;
  return item;
}

// -----------------------------------------------------------------------------
Item *Item::CreateFloat64(double val)
{
  Item *item = new Item(Kind::FLOAT64);
  item->float64val_ = val;
  return item;
}

// -----------------------------------------------------------------------------
Item *Item::CreateSpace(unsigned val)
{
  Item *item = new Item(Kind::SPACE);
  item->int32val_ = val;
  return item;
}

// -----------------------------------------------------------------------------
Item *Item::CreateExpr32(Expr *val)
{
  Item *item = new Item(Kind::EXPR32);
  new (&item->useVal_) Use(val, nullptr);
  return item;
}

// -----------------------------------------------------------------------------
Item *Item::CreateExpr64(Expr *val)
{
  Item *item = new Item(Kind::EXPR64);
  new (&item->useVal_) Use(val, nullptr);
  return item;
}

// -----------------------------------------------------------------------------
Item *Item::CreateString(const std::string_view str)
{
  Item *item = new Item(Kind::STRING);
  new (&item->stringVal_) std::string(str);
  return item;
}

// -----------------------------------------------------------------------------
Item::~Item()
{
  switch (kind_) {
    case Item::Kind::INT8:
    case Item::Kind::INT16:
    case Item::Kind::INT32:
    case Item::Kind::INT64:
    case Item::Kind::FLOAT64:
    case Item::Kind::SPACE: {
      return;
    }
    case Item::Kind::EXPR32:
    case Item::Kind::EXPR64: {
      if (auto *v = useVal_.get().Get()) {
        auto *expr = ::cast<Expr>(v);
        useVal_.~Use();
        if (expr->use_size() == 0) {
          delete expr;
        }
      } else {
        useVal_.~Use();
      }
      return;
    }
    case Item::Kind::STRING: {
      stringVal_.~basic_string();
      return;
    }
  }
  llvm_unreachable("invalid item kind");
}

// -----------------------------------------------------------------------------
size_t Item::GetSize() const
{
  switch (kind_) {
    case Item::Kind::INT8: return 1;
    case Item::Kind::INT16: return 2;
    case Item::Kind::INT32: return 4;
    case Item::Kind::INT64: return 8;
    case Item::Kind::FLOAT64: return 8;
    case Item::Kind::SPACE: return GetSpace();
    case Item::Kind::EXPR32: return 4;
    case Item::Kind::EXPR64: return 8;
    case Item::Kind::STRING: return GetString().size();
  }
  llvm_unreachable("invalid item kind");
}

// -----------------------------------------------------------------------------
void Item::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Item::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
Expr *Item::GetExpr()
{
  assert(kind_ == Kind::EXPR32 || kind_ == Kind::EXPR64);
  return &*::cast<Expr>(*useVal_);
}

// -----------------------------------------------------------------------------
const Expr *Item::GetExpr() const
{
  assert(kind_ == Kind::EXPR32 || kind_ == Kind::EXPR64);
  return &*::cast<Expr>(*useVal_);
}

// -----------------------------------------------------------------------------
Expr *Item::AsExpr()
{
  return IsExpr() ? GetExpr() : nullptr;
}

// -----------------------------------------------------------------------------
const Expr *Item::AsExpr() const
{
  return IsExpr() ? GetExpr() : nullptr;
}
