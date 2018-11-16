// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/data.h"
#include "core/func.h"
#include "core/block.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Prog::Prog()
  : data_(new Data)
  , bss_(new Data)
  , const_(new Data)
{
}

// -----------------------------------------------------------------------------
Func *Prog::AddFunc(const std::string_view name)
{
  auto it = symbols_.find(name);
  if (it != symbols_.end()) {
    assert(!"not implemented");
  } else {
    Func *f = new Func(this, name);
    funcs_.push_back(f);
    return f;
  }
}

// -----------------------------------------------------------------------------
Global *Prog::CreateSymbol(const std::string_view name)
{
  auto it = symbols_.emplace(name, nullptr);
  if (!it.second) {
    return it.first->second;
  }

  auto sym = new Symbol(name);
  it.first->second = sym;
  return sym;
}
