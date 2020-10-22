// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/expr.h"
#include "core/global.h"



// -----------------------------------------------------------------------------
Expr::~Expr()
{
}

// -----------------------------------------------------------------------------
SymbolOffsetExpr::SymbolOffsetExpr(Global *sym, int64_t offset)
  : Expr(Kind::SYMBOL_OFFSET, 1)
  , offset_(offset)
{
  Set<0>(sym);
}

// -----------------------------------------------------------------------------
const Global *SymbolOffsetExpr::GetSymbol() const
{
  return cast<Global>(Get<0>()).Get();
}

// -----------------------------------------------------------------------------
Global *SymbolOffsetExpr::GetSymbol()
{
  return cast<Global>(Get<0>()).Get();
}
