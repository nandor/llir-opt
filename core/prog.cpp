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
  Global *prev = nullptr;
  if (it != symbols_.end()) {
    prev = it->second;
    if (prev->IsDefinition()) {
      throw std::runtime_error("Overwriting definition");
    }
  }

  Func *f = new Func(this, name);
  funcs_.push_back(f);
  symbols_.emplace(f->GetName(), f);

  if (prev) {
    prev->replaceAllUsesWith(f);
  }
  return f;
}

// -----------------------------------------------------------------------------
void Prog::AddExternal(const std::string_view name)
{
  externs_.push_back(CreateSymbol(name));
}

// -----------------------------------------------------------------------------
Global *Prog::CreateSymbol(const std::string_view name)
{
  auto it = symbols_.find(name);
  if (it != symbols_.end()) {
    return it->second;
  }

  auto sym = new Symbol(name);
  symbols_.emplace(sym->GetName(), sym);
  return sym;
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Prog::const_ext_iterator> Prog::externs() const
{
  return llvm::make_range(ext_begin(), ext_end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Prog::ext_iterator> Prog::externs()
{
  return llvm::make_range(ext_begin(), ext_end());
}
