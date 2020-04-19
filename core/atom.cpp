// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/data.h"
#include "core/expr.h"


// -----------------------------------------------------------------------------
Item::Item(Atom *parent, const std::string_view str)
  : kind_(Kind::STRING)
  , parent_(parent)
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
    case Item::Kind::SPACE:
    case Item::Kind::END: {
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

// -----------------------------------------------------------------------------
Atom::~Atom()
{
}

// -----------------------------------------------------------------------------
void Atom::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
void Atom::erase(iterator it)
{
  items_.erase(it);
}

// -----------------------------------------------------------------------------
void Atom::AddAlignment(unsigned i)
{
  items_.push_back(new Item(this, Item::Align{ .V = i }));
}

// -----------------------------------------------------------------------------
void Atom::AddSpace(unsigned i)
{
  items_.push_back(new Item(this, Item::Space{ .V = i }));
}

// -----------------------------------------------------------------------------
void Atom::AddString(const std::string_view str)
{
  items_.push_back(new Item(this, str));
}

// -----------------------------------------------------------------------------
void Atom::AddInt8(int8_t v)
{
  items_.push_back(new Item(this, v));
}

// -----------------------------------------------------------------------------
void Atom::AddInt16(int16_t v)
{
  items_.push_back(new Item(this, v));
}

// -----------------------------------------------------------------------------
void Atom::AddInt32(int32_t v)
{
  items_.push_back(new Item(this, v));
}

// -----------------------------------------------------------------------------
void Atom::AddInt64(int64_t v)
{
  items_.push_back(new Item(this, v));
}

// -----------------------------------------------------------------------------
void Atom::AddFloat64(int64_t v)
{
  union { double d; int64_t v; } u = { .v = v };
  items_.push_back(new Item(this, u.d));
}

// -----------------------------------------------------------------------------
void Atom::AddFloat64(double v)
{
  items_.push_back(new Item(this, v));
}

// -----------------------------------------------------------------------------
void Atom::AddExpr(Expr *expr)
{
  items_.push_back(new Item(this, expr));
}

// -----------------------------------------------------------------------------
void Atom::AddSymbol(Global *global, int64_t off)
{
  items_.push_back(new Item(this, new SymbolOffsetExpr(global, off)));
}

// -----------------------------------------------------------------------------
void Atom::AddEnd()
{
  items_.push_back(new Item(this));
}

