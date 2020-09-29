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
      Visibility visibility = Visibility::HIDDEN
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
  llvm::Align GetAlignment() const override { return llvm::Align(1u); }

  /// Maps the extern to an alias.
  void SetAlias(Global *g) { Op<0>() = g; }
  /// Returns the alias, if it exists.
  Global *GetAlias()
  {
    return static_cast<Global *>(Op<0>().get());
  }
  /// Returns the alias, if it exists.
  const Global *GetAlias() const
  {
    return static_cast<const Global *>(Op<0>().get());
  }
  /// Checks if the extern is a weak alias to another symbol.
  bool HasAlias() const { return GetAlias() != nullptr; }

  /// Returns the program to which the extern belongs.
  Prog *getProg() override { return parent_; }

private:
  friend struct SymbolTableListTraits<Extern>;
  /// Updates the parent node.
  void setParent(Prog *parent) { parent_ = parent; }

private:
  /// Program containing the extern.
  Prog *parent_;
};
