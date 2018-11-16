// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <string_view>

#include "core/value.h"



/**
 * Base for global symbols.
 */
class Global : public Value {
public:
  /**
   * Enumeration of constant kinds.
   */
  enum Kind {
    SYMBOL,
    FUNC
  };

public:
  Global(Kind kind, const std::string_view name)
    : Value(Value::Kind::GLOBAL)
    , kind_(kind)
    , name_(name)
  {
  }

  virtual ~Global();

  /// Returns the kind of the global value.
  Kind GetKind() const { return kind_; }

  /// Returns the name of the global.
  const std::string &GetName() const { return name_; }

private:
  /// Returns the kind of the constant.
  Kind kind_;
  /// Name of the function.
  std::string name_;
};
