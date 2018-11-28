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
  Global(const std::string_view name, bool isDefined)
    : Value(Value::Kind::GLOBAL)
    , name_(name)
    , isDefined_(isDefined)
  {
  }

  virtual ~Global();

  /// Returns the name of the global.
  const std::string &GetName() const { return name_; }

  /// Checks if the global is a definition.
  virtual bool IsDefinition() const { return isDefined_; }

private:
  /// Name of the function.
  std::string name_;
  /// Flag indicating if the symbol is defined.
  bool isDefined_;
};
