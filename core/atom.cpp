// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/expr.h"



// -----------------------------------------------------------------------------
Item::~Item()
{
}

// -----------------------------------------------------------------------------
Atom::~Atom()
{
}

// -----------------------------------------------------------------------------
void Atom::AddAlignment(unsigned i)
{
  items_.push_back(new Item(Item::Align{ .V = i }));
}

// -----------------------------------------------------------------------------
void Atom::AddSpace(unsigned i)
{
  items_.push_back(new Item(Item::Space{ .V = i }));
}

// -----------------------------------------------------------------------------
void Atom::AddString(const std::string &str)
{
  items_.push_back(new Item(new std::string(str)));
}

// -----------------------------------------------------------------------------
void Atom::AddInt8(int8_t v)
{
  items_.push_back(new Item(v));
}

// -----------------------------------------------------------------------------
void Atom::AddInt16(int16_t v)
{
  items_.push_back(new Item(v));
}

// -----------------------------------------------------------------------------
void Atom::AddInt32(int32_t v)
{
  items_.push_back(new Item(v));
}

// -----------------------------------------------------------------------------
void Atom::AddInt64(int64_t v)
{
  items_.push_back(new Item(v));
}

// -----------------------------------------------------------------------------
void Atom::AddFloat64(int64_t v)
{
  union { double d; int64_t v; } u = { .v = v };
  items_.push_back(new Item(u.d));
}

// -----------------------------------------------------------------------------
void Atom::AddFloat64(double v)
{
  items_.push_back(new Item(v));
}

// -----------------------------------------------------------------------------
void Atom::AddExpr(Expr *expr)
{
  items_.push_back(new Item(expr));
}

// -----------------------------------------------------------------------------
void Atom::AddSymbol(Global *global, int64_t off)
{
  items_.push_back(new Item(new SymbolOffsetExpr(global, off)));
}

// -----------------------------------------------------------------------------
void Atom::AddEnd()
{
  items_.push_back(new Item());
}

