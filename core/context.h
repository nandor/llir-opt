// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>



/**
 * Interned symbol.
 */
class Symbol {
public:
};


/**
 * Context keeping track of some resources.
 */
class Context {
public:
  Context();

  Symbol *CreateSymbol(const std::string &sym);
private:

};
