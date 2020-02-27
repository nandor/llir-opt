// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>
#include "core/adt/id.h"

class Func;
class Inst;
class LCGraph;
class LCSet;
class LCAlloc;



/**
 * Mapping from IR to LC state for a function.
 */
class LCContext final {
public:
  /// Initialises the context.
  LCContext(Func &func, LCGraph &graph);

  /// Returns the graph.
  LCGraph &Graph();

  /// Set of external nodes.
  LCSet *Extern();

  /// Set of root nodes.
  LCSet *Root();

  /// Allocation for a frame node.
  LCAlloc *Frame(unsigned obj);

  /// Maps an instruction to a specific node.
  LCSet *MapNode(const Inst *inst, LCSet *node);
  /// Returns the node mapped to an instruction.
  LCSet *GetNode(const Inst *inst);

  /// Creates a live set for a node.
  LCSet *MapLive(const Inst *inst, LCSet *node);
  /// Returns the live set for a node.
  LCSet *GetLive(const Inst *inst);

private:
  /// Parent function.
  Func &func_;
  /// Constraint graph.
  LCGraph &graph_;
  /// Extern node ID.
  ID<LCSet> extern_;
  /// Heap root ID.
  ID<LCSet> root_;
  /// Mapping from instructions to nodes.
  std::unordered_map<const Inst *, ID<LCSet>> nodes_;
  /// Mapping from instructions to live sets.
  std::unordered_map<const Inst *, ID<LCSet>> lives_;
  /// The identifier of the frame node.
  llvm::DenseMap<unsigned, LCAlloc *> frame_;
};
