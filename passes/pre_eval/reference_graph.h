// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_map>

class Global;
class Func;
class Prog;



/**
 * Class caching the set of symbols transitively referenced by a function.
 */
class ReferenceGraph final {
public:
  /// Information about this node.
  struct Node {
    /// Flag to indicate whether any reachable node has indirect calls.
    bool HasIndirectCalls;
    /// Flag to indicate whether any reachable node raises.
    bool HasRaise;
    /// Set of referenced symbols.
    std::set<Global *> Referenced;
  };

  /// Build reference information.
  ReferenceGraph(Prog &prog);

  /// Return the set of globals referenced by a function.
  Node *FindReferences(Func *func);
};
