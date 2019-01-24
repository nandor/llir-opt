// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <string_view>
#include <llvm/ADT/StringRef.h>

#include "core/value.h"



/**
 * Base for global symbols.
 */
class Global : public Value {
public:
  /// Parent type.
  using ParentType = Value;
  /// Kind of the instruction.
  static constexpr ParentType::Kind kKind = ParentType::Kind::GLOBAL;

public:
  /// Enumeration of global kinds.
  enum class Kind {
    SYMBOL,
    EXTERN,
    FUNC,
    BLOCK,
    ATOM
  };

public:
  Global(Kind kind, const std::string_view name, bool defined)
    : Value(Value::Kind::GLOBAL)
    , kind_(kind)
    , name_(name)
    , defined_(defined)
  {
  }

  virtual ~Global();

  /// Returns the kind of the global.
  Kind GetKind() const { return kind_; }
  /// Checks if the global is of a specific kind.
  bool Is(Kind kind) const { return GetKind() == kind; }

  /// Returns the name of the global.
  const std::string_view GetName() const { return name_; }
  /// Returns the name of the basic block for LLVM.
  llvm::StringRef getName() const { return name_; }

  /// Checks if the global is a definition.
  bool IsDefined() const { return defined_; }

private:
  /// Kind of the global.
  Kind kind_;
  /// Name of the function.
  std::string name_;
  /// Flag indicating if the symbol is defined.
  bool defined_;
};
