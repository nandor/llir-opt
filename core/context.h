// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

class Expr;



/**
 * Interned symbol.
 */
class Symbol {
public:
  /**
   * Creates a new symbol.
   */
  Symbol(const std::string &name) : name_(name) {}

  /**
   * Returns the name of the symbol.
   */
  const std::string &GetName() const { return name_; }

private:
  /// Name of the symbol.
  const std::string name_;
};


/**
 * Context keeping track of some resources.
 */
class Context {
public:
  /**
   * Initialises the context.
   */
  Context();

  /**
   * Creates a new interned symbol.
   */
  Symbol *CreateSymbol(const std::string &name);

  /**
   * Creates a new symbol offset expression.
   */
  Expr *CreateSymbolOffset(Symbol *sym, int64_t offset);

private:
  /// Map from names to interned symbols.
  std::unordered_map<std::string_view, std::unique_ptr<Symbol>> symbols_;
};
