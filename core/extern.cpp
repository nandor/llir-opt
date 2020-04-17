// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/extern.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Extern::~Extern()
{
}

// -----------------------------------------------------------------------------
void Extern::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}
