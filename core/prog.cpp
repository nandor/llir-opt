// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/data.h"
#include "core/func.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Prog::Prog()
  : data_(new Data)
  , bss_(new Data)
  , const_(new Data)
{
}

// -----------------------------------------------------------------------------
Func *Prog::AddFunc(const std::string &str)
{
  return new Func(this, str);
}
