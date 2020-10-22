// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/extern.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Extern::Extern(
    const std::string_view name,
    Visibility visibility)
  : Global(Global::Kind::EXTERN, name, visibility, 1)
  , parent_(nullptr)
{
}

// -----------------------------------------------------------------------------
Extern::Extern(
    const std::string_view name,
    const std::string_view section,
    Visibility visibility)
  : Global(Global::Kind::EXTERN, name, visibility, 1)
  , section_(section)
  , parent_(nullptr)
{
}

// -----------------------------------------------------------------------------
Extern::~Extern()
{
}

// -----------------------------------------------------------------------------
void Extern::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Extern::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
void Extern::SetAlias(Global *g)
{
  Set<0>(g);
}

// -----------------------------------------------------------------------------
Global *Extern::GetAlias()
{
  return cast_or_null<Global>(Get<0>()).Get();
}

// -----------------------------------------------------------------------------
const Global *Extern::GetAlias() const
{
  return cast_or_null<Global>(Get<0>()).Get();
}
