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
Expr *Context::CreateSymbolOffset(Symbol *sym, int64_t offset)
{
  return new SymbolOffsetExpr(sym, offset);
}
