// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>

#include "core/global.h"



/**
 * Interned symbol.
 */
class Symbol final : public Global {
public:
  /**
   * Creates a new symbol.
   */
  Symbol(const std::string_view name, bool isDefined = false)
    : Global(name, isDefined)
  {
  }

  /**
   * Frees the symbol.
   */
  ~Symbol() override;
};


/**
 * External symbol.
 */
class Extern final : public Global {
public:
  /**
   * Creates a new extern.
   */
  Extern(const std::string_view name)
    : Global(name, true)
  {
  }

  /**
   * Frees the symbol.
   */
  ~Extern() override;
};
