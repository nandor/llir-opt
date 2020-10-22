// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>
#include <set>
#include <stack>

class Func;
class Prog;
class Value;


/**
 * Graph for functions which require trampolines
 */
class TrampolineGraph final {
public:
  /// Construct a trampoline graph for a program.
  TrampolineGraph(const Prog *prog);

  /// Checks whether a call to a specific callee needs a trampoline.
  bool NeedsTrampoline(ConstRef<Value> callee);

private:

  /// Builds the graph for the whole program.
  void BuildGraph(const Prog *prog);

  /// Explores a function.
  void Visit(const Func *func);

private:
  /// Graph node.
  struct Node {
    std::set<const Func *> Out;
    unsigned Index = 0;
    unsigned LowLink = 0;
    bool OnStack = false;
    bool Trampoline = false;
  };
  /// Call graph.
  std::unordered_map<const Func *, Node> graph_;

  /// SCC index.
  unsigned index_ = 1;
  /// SCC stack.
  std::stack<const Func *> stack_;
};
