// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <llvm/ADT/ilist_node.h>

#include "core/global.h"

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
  Extern(Prog *prog, const std::string_view name)
    : Global(Global::Kind::EXTERN, name)
    , prog_(prog)
  {
  }

  /**
   * Frees the symbol.
   */
  ~Extern() override;

  /// Returns the parent node.
  Prog *getParent() { return prog_; }

  /// Removes the extern from the parent.
  void eraseFromParent();

  /// Externs have no known alignment.
  unsigned GetAlignment() const override { return 1u; }

private:
  /// Program containing the extern.
  Prog *prog_;
};
