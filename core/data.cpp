// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/data.h"
#include "core/prog.h"
#include "core/symbol.h"



// -----------------------------------------------------------------------------
void Data::Align(unsigned i)
{
  GetAtom()->AddItem(new Item(Item::Kind::ALIGN, i));
}

// -----------------------------------------------------------------------------
void Data::AddSpace(unsigned i)
{
  GetAtom()->AddItem(new Item(Item::Kind::SPACE, i));
}

// -----------------------------------------------------------------------------
void Data::AddString(const std::string &str)
{
  GetAtom()->AddItem(new Item(Item::Kind::STRING, new std::string(str)));
}

// -----------------------------------------------------------------------------
void Data::AddInt8(int8_t v)
{
  GetAtom()->AddItem(new Item(Item::Kind::INT8, v));
}

// -----------------------------------------------------------------------------
void Data::AddInt16(int16_t v)
{
  GetAtom()->AddItem(new Item(Item::Kind::INT16, v));
}

// -----------------------------------------------------------------------------
void Data::AddInt32(int32_t v)
{
  GetAtom()->AddItem(new Item(Item::Kind::INT32, v));
}

// -----------------------------------------------------------------------------
void Data::AddInt64(int64_t v)
{
  GetAtom()->AddItem(new Item(Item::Kind::INT64, v));
}

// -----------------------------------------------------------------------------
void Data::AddFloat64(int64_t v)
{
  GetAtom()->AddItem(new Item(Item::Kind::FLOAT64, v));
}

// -----------------------------------------------------------------------------
void Data::AddSymbol(Global *global, int64_t offset)
{
  GetAtom()->AddItem(new Item(Item::Kind::SYMBOL, global, offset));
}

// -----------------------------------------------------------------------------
void Data::AddEnd()
{
  GetAtom()->AddItem(new Item(Item::Kind::END));
}

// -----------------------------------------------------------------------------
Atom *Data::GetAtom()
{
  if (atoms_.empty()) {
    Atom *atom = new Atom(name_ + "$begin");
    atoms_.push_back(atom);
    return atom;
  } else {
    return &*atoms_.rbegin();
  }
}

// -----------------------------------------------------------------------------
Atom *Data::CreateAtom(const std::string_view name)
{
  Atom *atom = prog_->CreateAtom(name);
  atoms_.push_back(atom);
  return atom;
}
