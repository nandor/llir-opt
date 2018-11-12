// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/data.h"
#include "core/symbol.h"



// -----------------------------------------------------------------------------
void Data::Align(unsigned i)
{
}

// -----------------------------------------------------------------------------
void Data::AddInt8(Const *v)
{
}

// -----------------------------------------------------------------------------
void Data::AddInt16(Const *v)
{
}

// -----------------------------------------------------------------------------
void Data::AddInt32(Const *v)
{
}

// -----------------------------------------------------------------------------
void Data::AddInt64(Const *v)
{
}

// -----------------------------------------------------------------------------
void Data::AddFloat64(Const *v)
{
}

// -----------------------------------------------------------------------------
void Data::AddZero(Const *v)
{
}

// -----------------------------------------------------------------------------
Symbol *Data::CreateSymbol(const std::string_view name)
{
  auto it = symbolMap_.find(name);
  if (it != symbolMap_.end()) {
    return it->second.get();
  }

  auto sym = std::make_unique<Symbol>(name);
  auto atom = std::make_unique<Atom>(sym.get());

  atoms_.push_back(atom.get());
  atomMap_.emplace(sym.get(), std::move(atom));
  auto st = symbolMap_.emplace(sym->GetName(), std::move(sym));
  return st.first->second.get();
}
