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
  TrampolineGraph(const Prog *prog);

  bool NeedsTrampoline(const Value *callee);

private:

  void BuildGraph(const Prog *prog);

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
