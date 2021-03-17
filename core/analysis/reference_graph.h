// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <memory>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class Block;
class CallGraph;
class Func;
class Global;
class Prog;
class MovInst;
class Object;



/**
 * Set of start-end offsets for stores and loads.
 */
using OffsetSet = std::set<std::pair<int64_t, int64_t>>;

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
    std::unordered_set<Object *> ReadRanges;
    /// Set of referenced offsets in objects.
    std::unordered_map<Object *, OffsetSet> ReadOffsets;
    /// Set of written symbols.
    std::unordered_set<Object *> WrittenRanges;
    /// Set of written offsets in symbols.
    std::unordered_map<Object *, OffsetSet> WrittenOffsets;
    /// Set of symbols which escape.
    std::unordered_set<Global *> Escapes;
    /// Set of called functions.
    std::unordered_set<Func *> Called;
    /// Set of addressed blocks.
    std::unordered_set<Block *> Blocks;

    /// Merge another node into this one.
    void Merge(const Node &that);
    /// Add an inaccurate read.
    void AddRead(Object *object);
    /// Add an inaccurate write.
    void AddWrite(Object *object);
  };

  /// Build reference information.
  ReferenceGraph(Prog &prog, CallGraph &graph);

  /// Return the set of globals referenced by a function.
  const Node &operator[](Func &func);

protected:
  /// Callback which decides whether to follow or skip a function.
  virtual bool Skip(Func &func) { return false; }

private:
  /// Extract the properties of a single function.
  void ExtractReferences(Func &func, Node &node);
  /// Build the graph.
  void Build();
  /// Classify the use, without allowing accurate offsets.
  void Classify(Object *o, const MovInst &inst, Node &node);
  /// Classify the use, possibly marking accurate reads/writes.
  void Classify(Object *o, const MovInst &inst, Node &node, int64_t offset);

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
