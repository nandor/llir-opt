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
  Symbol(const std::string_view name)
    : Global(Global::Kind::SYMBOL, name)
  {
  }

  /**
   * Frees the symbol.
   */
  ~Symbol() override;

  /// Symbols are not definitions.
  bool IsDefinition() const override { return false; }
};
