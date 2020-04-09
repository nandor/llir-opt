// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"



// -----------------------------------------------------------------------------
Item::~Item()
{
  assert(!"not implemented");
}


// -----------------------------------------------------------------------------
Atom::~Atom()
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void Atom::AddAlignment(unsigned i)
{
  items_.push_back(new Item(Item::Kind::ALIGN, i));
}

// -----------------------------------------------------------------------------
void Atom::AddSpace(unsigned i)
{
  items_.push_back(new Item(Item::Kind::SPACE, i));
}

// -----------------------------------------------------------------------------
void Atom::AddString(const std::string &str)
{
  items_.push_back(new Item(Item::Kind::STRING, new std::string(str)));
}

// -----------------------------------------------------------------------------
void Atom::AddInt8(int8_t v)
{
  items_.push_back(new Item(Item::Kind::INT8, v));
}

// -----------------------------------------------------------------------------
void Atom::AddInt16(int16_t v)
{
  items_.push_back(new Item(Item::Kind::INT16, v));
}

// -----------------------------------------------------------------------------
void Atom::AddInt32(int32_t v)
{
  items_.push_back(new Item(Item::Kind::INT32, v));
}

// -----------------------------------------------------------------------------
void Atom::AddInt64(int64_t v)
{
  items_.push_back(new Item(Item::Kind::INT64, v));
}

// -----------------------------------------------------------------------------
void Atom::AddFloat64(int64_t v)
{
  items_.push_back(new Item(Item::Kind::FLOAT64, v));
}

// -----------------------------------------------------------------------------
void Atom::AddSymbol(Global *global, int64_t offset)
{
  items_.push_back(new Item(Item::Kind::SYMBOL, global, offset));
}

// -----------------------------------------------------------------------------
void Atom::AddEnd()
{
  items_.push_back(new Item(Item::Kind::END));
}

