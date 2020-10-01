// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/extern.h"
#include "core/prog.h"


// -----------------------------------------------------------------------------
Extern::Extern(
    const std::string_view name,
    Visibility visibility,
    bool defined)
  : Global(Global::Kind::EXTERN, name, visibility, 1)
  , parent_(nullptr)
  , defined_(defined)
{
  Op<0>() = nullptr;
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
