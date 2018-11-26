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

// -----------------------------------------------------------------------------
ConstantInt *Context::CreateInt(int64_t v)
{
  return new ConstantInt(v);
}

// -----------------------------------------------------------------------------
ConstantFloat *Context::CreateFloat(double v)
{
  return new ConstantFloat(v);
}

// -----------------------------------------------------------------------------
ConstantReg *Context::CreateReg(ConstantReg::Kind v)
{
  return new ConstantReg(v);
}
