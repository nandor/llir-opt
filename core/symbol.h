// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>

#include "core/value.h"



/**
 * Interned symbol.
 */
class Symbol final : public Value {
public:
  /**
   * Creates a new symbol.
   */
  Symbol(const std::string_view name)
    : Value(Value::Kind::SYMBOL)
    , name_(name)
  {
  }

  /**
   * Frees the symbol.
   */
  ~Symbol();

  /**
   * Returns the name of the symbol.
   */
  const std::string &GetName() const { return name_; }

private:
  /// Name of the symbol.
  const std::string name_;
};
