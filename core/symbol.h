// This file if part of the llir-opt project.
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
    : Global(Global::Kind::SYMBOL, name, false)
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
  /// Kind of the global.
  static constexpr Global::Kind kGlobalKind = Global::Kind::EXTERN;

public:
  /**
   * Creates a new extern.
   */
  Extern(const std::string_view name)
    : Global(Global::Kind::EXTERN, name, true)
  {
  }

  /**
   * Frees the symbol.
   */
  ~Extern() override;
};
