// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/ilist_node.h>

#include "core/value.h"
#include "core/symbol.h"

class Data;



/**
 * Class representing a value in the data section.
 */
class Const {
public:
};

/**
 * Class representing an integer value.
 */
class IntValue : public Const {
public:
  IntValue(int val)
  {
  }
};

/**
 * Class representing a symbol value.
 */
class SymValue : public Const, public User {
public:
  SymValue(Global *sym, int64_t offset)
    : User(1)
  {
    Op<0>() = sym;
  }
};



/**
 * Data atom, a symbol followed by some data.
 */
class Atom : public llvm::ilist_node_with_parent<Atom, Data>, public User {
public:
  /**
   * Creates a new atom.
   */
  Atom(Symbol *sym)
    : User(1)
  {
    Op<0>() = sym;
  }

  /// Returns the symbol attached to the atom.
  Symbol *GetSymbol() const { return static_cast<Symbol *>(Op<0>().get()); }
};
