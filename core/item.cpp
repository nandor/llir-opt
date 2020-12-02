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
    case Item::Kind::ALIGN:
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
void Item::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}
