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
Func *Prog::AddFunc(const std::string &str)
{
  Func *f = new Func(this, str);
  funcs_.push_back(f);
  return f;
}

// -----------------------------------------------------------------------------
Symbol *Prog::CreateSymbol(const std::string_view name)
{
  auto it = symbols_.find(name);
  if (it != symbols_.end()) {
    return it->second.get();
  }

  auto sym = std::make_unique<Symbol>(name);
  auto jt = symbols_.emplace(sym->GetName(), std::move(sym));
  return jt.first->second.get();
}
