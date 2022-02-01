// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <llvm/ADT/ilist_node.h>

#include "core/global.h"
#include "core/symbol_table.h"

class Prog;



/**
 * External symbol.
 */
class Extern final : public llvm::ilist_node_with_parent<Extern, Prog>, public Global {
public:
  /// Kind of the global.
  static constexpr Global::Kind kGlobalKind = Global::Kind::EXTERN;

public:
  /**
   * Creates a new extern.
   */
  Extern(
      const std::string_view name,
      Visibility visibility = Visibility::GLOBAL_DEFAULT
  );

  /**
   * Creates a new extern in a section.
   */
  Extern(
      const std::string_view name,
      const std::string_view section,
      Visibility visibility = Visibility::GLOBAL_DEFAULT
  );

  /**
   * Frees the symbol.
   */
  ~Extern() override;

  /// Returns the parent node.
  Prog *getParent() { return parent_; }

  /// Removes an extern from the parent.
  void removeFromParent() override;
  /// Erases the extern from the parent, deleting it.
  void eraseFromParent() override;

  /// Externs have no known alignment.
  std::optional<llvm::Align> GetAlignment() const override
  {
    return std::nullopt;
  }

  /// Maps the extern to an alias.
  void SetValue(Ref<Value> g);
  /// Returns the alias, if it exists.
  Ref<Value> GetValue() { return Get<0>(); }
  /// Returns the alias, if it exists.
  ConstRef<Value> GetValue() const { return Get<0>(); }

  /// Checks if the extern is a weak alias to another symbol.
  bool HasValue() const { return GetValue(); }

  /// Returns the program to which the extern belongs.
  Prog *getProg() override { return parent_; }

  /// Sets the section of the extern.
  void SetSection(const std::string_view section) { section_ = section; }
  /// Returns the section.
  std::optional<const std::string_view> GetSection() const { return section_; }

  /// Dumps the representation of the function.
  void dump(llvm::raw_ostream &os = llvm::errs()) const override;

private:
  friend struct SymbolTableListTraits<Extern>;
  /// Updates the parent node.
  void setParent(Prog *parent) { parent_ = parent; }

private:
  /// Program containing the extern.
  Prog *parent_;
  /// Section where the symbol is located.
  std::optional<std::string> section_;
};
