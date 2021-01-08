// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/item.h"
#include "core/atom.h"
#include "core/expr.h"
#include "core/cast.h"


// -----------------------------------------------------------------------------
Item::Item(Expr *val)
  : kind_(Kind::EXPR)
  , parent_(nullptr)
{
  new (&useVal_) Use(val, nullptr);
}

// -----------------------------------------------------------------------------
Item::Item(const std::string_view str)
  : kind_(Kind::STRING)
  , parent_(nullptr)
{
  new (&stringVal_) std::string(str);
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
    case Item::Kind::EXPR: {
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
    case Item::Kind::EXPR: return 8;
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
  assert(kind_ == Kind::EXPR);
  return &*::cast<Expr>(*useVal_);
}

// -----------------------------------------------------------------------------
const Expr *Item::GetExpr() const
{
  assert(kind_ == Kind::EXPR);
  return &*::cast<Expr>(*useVal_);
}

// -----------------------------------------------------------------------------
Expr *Item::AsExpr()
{
  return kind_ == Kind::EXPR ? GetExpr() : nullptr;
}

// -----------------------------------------------------------------------------
const Expr *Item::AsExpr() const
{
  return kind_ == Kind::EXPR ? GetExpr() : nullptr;
}
