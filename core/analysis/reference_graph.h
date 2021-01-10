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
class ReferenceGraph {
public:
  /// Information about this node.
  struct Node {
    /// Flag to indicate whether any reachable node has indirect calls.
    bool HasIndirectCalls = false;
    /// Flag to indicate whether any reachable node raises.
    bool HasRaise = false;
    /// Check whether there are barriers.
    bool HasBarrier = false;
    /// Set of referenced symbols.
    std::set<Global *> Referenced;
    /// Set of called functions.
    std::set<Func *> Called;
  };

  /// Build reference information.
  ReferenceGraph(Prog &prog, CallGraph &graph);

  /// Return the set of globals referenced by a function.
  const Node &FindReferences(Func &func);

protected:
  /// Callback which decides whether to follow or skip a function.
  virtual bool Skip(Func &func) { return false; }

private:
  /// Extract the properties of a single function.
  void ExtractReferences(Func &func, Node &node);
  /// Build the graph.
  void Build();

private:
  /// Call graph of the program.
  CallGraph &graph_;
  /// Mapping from functions to nodes.
  std::unordered_map<Func *, Node *> funcToNode_;
  /// List of all nodes.
  std::vector<std::unique_ptr<Node>> nodes_;
  /// Flag to indicate whether graph was built.
  bool built_;
};
