// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

class Global;
class Func;
class Prog;
class CallGraph;



/**
 * Class caching the set of symbols transitively referenced by a function.
 */
class ReferenceGraph final {
public:
  /// Information about this node.
  struct Node {
    /// Flag to indicate whether any reachable node has indirect calls.
    bool HasIndirectCalls = false;
    /// Flag to indicate whether any reachable node raises.
    bool HasRaise = false;
    /// Set of referenced symbols.
    std::set<Global *> Referenced;
  };

  /// Build reference information.
  ReferenceGraph(Prog &prog, CallGraph &graph);

  /// Return the set of globals referenced by a function.
  const Node &FindReferences(Func &func) { return *funcToNode_[&func]; }

private:
  /// Extract the properties of a single function.
  void ExtractReferences(Func &func, Node &node);

private:
  /// Call graph of the program.
  CallGraph &graph_;
  /// Mapping from functions to nodes.
  std::unordered_map<Func *, Node *> funcToNode_;
  /// List of all nodes.
  std::vector<std::unique_ptr<Node>> nodes_;
};
