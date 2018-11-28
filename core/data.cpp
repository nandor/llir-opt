// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/data.h"
#include "core/prog.h"
#include "core/symbol.h"



// -----------------------------------------------------------------------------
void Data::Align(unsigned i)
{
}

// -----------------------------------------------------------------------------
void Data::AddSpace(unsigned i)
{
}

// -----------------------------------------------------------------------------
void Data::AddString(const std::string &str)
{

}

// -----------------------------------------------------------------------------
void Data::AddInt8(Const *v)
{
  assert(v && "invalid value");
}

// -----------------------------------------------------------------------------
void Data::AddInt16(Const *v)
{
  assert(v && "invalid value");
}

// -----------------------------------------------------------------------------
void Data::AddInt32(Const *v)
{
  assert(v && "invalid value");
}

// -----------------------------------------------------------------------------
void Data::AddInt64(Const *v)
{
  assert(v && "invalid value");
}

// -----------------------------------------------------------------------------
void Data::AddFloat64(Const *v)
{
  assert(v && "invalid value");
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
