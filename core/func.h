// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include "adt/chain.h"

class Prog;
class Block;



/**
 * GenericMachine function.
 */
class Func final : ChainNode<Func> {
public:
  /**
   * Creates a new function.
   */
  Func(Prog *prog, const std::string &name);

  /**
   * Adds a new basic block.
   */
  Block *AddBlock(const std::string &name);

  /**
   * Adds a new anonymous basic block.
   */
  Block *AddBlock();

private:
  /// Name of the underlying program.
  Prog *prog_;
  /// Name of the function.
  std::string name_;
};
