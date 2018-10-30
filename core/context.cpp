// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <memory>
#include "core/context.h"
#include "core/expr.h"



// -----------------------------------------------------------------------------
Context::Context()
{
}

// -----------------------------------------------------------------------------
Symbol *Context::CreateSymbol(const std::string &name)
{
  auto it = symbols_.find(name);
  if (it != symbols_.end()) {
    return it->second.get();
  }

  auto sym = std::make_unique<Symbol>(name);
  auto jt = symbols_.emplace(sym->GetName(), std::move(sym));
  return jt.first->second.get();
}

// -----------------------------------------------------------------------------
Expr *Context::CreateSymbolOffset(Symbol *sym, int64_t offset)
{
  return new SymbolOffsetExpr(sym, offset);
}
