// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <string_view>
#include <llvm/ADT/StringRef.h>

#include "core/value.h"
#include "core/visibility.h"

class Prog;

class Prog;



/**
 * Base for global symbols.
 */
class Global : public Value {
public:
  /// Kind of the global.
  static constexpr Value::Kind kValueKind = Value::Kind::GLOBAL;

public:
  /// Enumeration of global kinds.
  enum class Kind {
    EXTERN,
    FUNC,
    BLOCK,
    ATOM
  };

public:
  Global(Kind kind, const std::string_view name)
    : Value(Value::Kind::GLOBAL)
    , kind_(kind)
    , name_(name)
    , visibility_(Visibility::HIDDEN)
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

  /// Externs have no known alignment.
  virtual unsigned GetAlignment() const = 0;

  /// Sets the visibilty of the function.
  void SetVisibility(Visibility visibility) { visibility_ = visibility; }
  /// Returns the visibilty of a function.
  Visibility GetVisibility() const { return visibility_; }
  /// Checks if a symbol is hidden.
  bool IsHidden() const { return GetVisibility() == Visibility::HIDDEN; }
  /// Checks if a symbol is weak.
  bool IsWeak() const { return GetVisibility() == Visibility::WEAK; }


  /// Removes the global from the parent container.
  virtual void removeFromParent() = 0;
  /// Removes the global from the parent container, deleting it.
  virtual void eraseFromParent() = 0;

private:
  friend class Prog;
  /// Kind of the global.
  Kind kind_;
  /// Name of the global.
  std::string name_;
  /// Visibility of the global.
  Visibility visibility_;
};
