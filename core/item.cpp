// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/item.h"
#include "core/atom.h"
#include "core/expr.h"



// -----------------------------------------------------------------------------
Item::Item(const std::string_view str)
  : kind_(Kind::STRING)
  , parent_(nullptr)
  , stringVal_(new std::string(str))
{
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
      delete exprVal_;
      return;
    }
    case Item::Kind::STRING: {
      delete stringVal_;
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
void Item::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}
