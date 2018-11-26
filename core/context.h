// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>

#include "core/constant.h"

class Expr;
class Symbol;



/**
 * Context keeping track of some resources.
 */
class Context {
public:
  /**
   * Initialises the context.
   */
  Context();

  /**
   * Creates a new symbol offset expression.
   */
  Expr *CreateSymbolOffset(Symbol *sym, int64_t offset);

  /**
   * Returns an integer value.
   */
  ConstantInt *CreateInt(int64_t v);

  /**
   * Returns a float value.
   */
  ConstantFloat *CreateFloat(double v);

  /**
   * Returns a register value.
   */
  ConstantReg *CreateReg(ConstantReg::Kind v);
};
