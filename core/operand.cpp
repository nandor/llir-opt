// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/operand.h"
#include "core/inst.h"
#include "core/value.h"
#include "core/symbol.h"
#include "core/block.h"



// -----------------------------------------------------------------------------
bool Operand::IsInst() const
{
  return IsValue() && valueData_->Is(Value::Kind::INST);
}

// -----------------------------------------------------------------------------
bool Operand::IsSym() const
{
  return IsValue() && valueData_->Is(Value::Kind::SYMBOL);
}

// -----------------------------------------------------------------------------
bool Operand::IsExpr() const
{
  return IsValue() && valueData_->Is(Value::Kind::EXPR);
}

// -----------------------------------------------------------------------------
bool Operand::IsBlock() const
{
  return IsValue() && valueData_->Is(Value::Kind::BLOCK);
}

// -----------------------------------------------------------------------------
Inst *Operand::GetInst() const
{
  assert(IsInst());
  return static_cast<Inst *>(valueData_);
}

// -----------------------------------------------------------------------------
Symbol *Operand::GetSym() const
{
  assert(IsSym());
  return static_cast<Symbol *>(valueData_);
}

// -----------------------------------------------------------------------------
Expr *Operand::GetExpr() const
{
  assert(IsExpr());
  return static_cast<Expr *>(valueData_);
}

// -----------------------------------------------------------------------------
Block *Operand::GetBlock() const
{
  assert(IsBlock());
  return static_cast<Block *>(valueData_);
}
