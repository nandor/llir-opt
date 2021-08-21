// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_set>

class Func;
class Block;
class CallGraph;



/**
 * Analysis which identifies which blocks are executed at most once.
 */
class InitPath final {
public:
  /// Build the path from the entry node.
  InitPath(Prog &prog, Func *entry);

  /// Check if a block is on the init path.
  bool operator[] (Block &block) const
  {
    return executedAtMostOnce_.count(&block) == 1;
  }

private:
  /// Returns true if the function has a unique use site.
  bool IsSingleUse(const Func *f);

private:
  /// Set of unknown/looping blocks.
  std::unordered_set<Block *> executedAtMostOnce_;
  /// Cache of single-use functions.
  std::unordered_map<const Func *, bool> singleUse_;
};
