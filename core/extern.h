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
  Extern(const std::string_view name)
    : Global(Global::Kind::EXTERN, name)
    , parent_(nullptr)
  {
  }

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
  unsigned GetAlignment() const override { return 1u; }

private:
  friend struct SymbolTableListTraits<Extern>;
  /// Updates the parent node.
  void setParent(Prog *parent) { parent_ = parent; }

private:
  /// Program containing the extern.
  Prog *parent_;
};
