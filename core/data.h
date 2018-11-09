// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include <llvm/ADT/ilist_node.h>
#include "llvm/ADT/ilist.h"

#include "core/symbol.h"

class Data;



/**
 * Class representing a value in the data section.
 */
class Value {
public:
};

/**
 * Class representing an integer value.
 */
class IntValue : public Value {
public:
};

/**
 * Class representing a symbol value.
 */
class SymValue : public Value {
public:
};

/**
 * Class representing an expression value.
 */
class ExprValue : public Value {
public:
};




/**
 * Data atom, a symbol followed by some data.
 */
class Atom : public llvm::ilist_node_with_parent<Atom, Data> {
public:
  /**
   * Creates a new atom.
   */
  Atom(Symbol *sym) : sym_(sym) {}

  /// Returns the symbol attached to the atom.
  Symbol *GetSymbol() const { return sym_; }

private:
  /// Symbol identifying the atom.
  Symbol *sym_;
};

/**
 * The data segment of a program.
 */
class Data {
private:
  /// Type of the function list.
  using AtomListType = llvm::ilist<Atom>;

  /// Iterator over the functions.
  using iterator = AtomListType::iterator;
  using const_iterator = AtomListType::const_iterator;

public:
  // Methods to populate atoms.
  void Align(unsigned i);
  void AddInt8(Value *v);
  void AddInt16(Value *v);
  void AddInt32(Value *v);
  void AddInt64(Value *v);
  void AddFloat64(Value *v);
  void AddZero(Value *v);

  /// Adds a symbol and an atom to the segment.
  Symbol *CreateSymbol(const std::string_view name);

  // Iterators over atoms.
  iterator begin() { return atoms_.begin(); }
  iterator end() { return atoms_.end(); }
  const_iterator begin() const { return atoms_.begin(); }
  const_iterator end() const { return atoms_.end(); }

private:
  /// List of atoms in the program.
  AtomListType atoms_;
  /// Mapping from names to symbols.
  std::unordered_map<std::string_view, std::unique_ptr<Symbol>> symbolMap_;
  /// Mapping form symbols to atoms.
  std::unordered_map<Symbol *, std::unique_ptr<Atom>> atomMap_;
};
