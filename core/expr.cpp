// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

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
  Op<0>() = sym;
}

// -----------------------------------------------------------------------------
Global *SymbolOffsetExpr::GetSymbol() const
{
  return static_cast<Global *>(Op<0>().get());
}
