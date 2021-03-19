// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/adt/bitset.h"
#include "core/analysis/reference_graph.h"
#include "core/dag.h"
#include "core/ref.h"
#include "core/type.h"

class Object;
class Inst;
class MemoryStoreInst;



/// Map of offsets into an object.
using ObjectOffsetMap = std::unordered_map<ID<Object>, OffsetSet>;

/// Transitive closure of an object.
struct ObjectClosure {
  /// Set of referenced functions.
  BitSet<Func> Funcs;
  /// Set of referenced objects.
  BitSet<Object> Objects;
};

/// Transitive closure of a function.
struct FuncClosure {
  /// DAG-based representation of the function.
  std::unique_ptr<DAGFunc> DAG;
  /// Set of referenced functions.
  BitSet<Func> Funcs;
  /// Set of escaped objects.
  BitSet<Object> Escaped;
  /// Set of changed objects.
  BitSet<Object> Stored;
  /// Set of dereferenced objects.
  BitSet<Object> Loaded;
  /// Flag to indicate whether any function raises.
  bool Raises;
  /// Flag to indicate whether any function has indirect calls.
  bool Indirect;
};

/// Evaluation state of a node.
struct NodeState {
  /// IDs of referenced functions.
  BitSet<Func> Funcs;
  /// ID of tainted objects.
  BitSet<Object> Escaped;
  /// Set of objects changed to unknown values.
  BitSet<Object> Stored;
  /// Accurate stores.
  std::unordered_map
    < ID<Object>
    , std::map<uint64_t, std::pair<Type, Ref<Inst>>>
    > Stores;

  void Merge(const NodeState &that);

  void Overwrite(ID<Object> changed)
  {
    BitSet<Object> imprecise;
    imprecise.Insert(changed);
    return Overwrite(imprecise);
  }

  void Overwrite(const BitSet<Object> &changed);

  void dump(llvm::raw_ostream &os);
};


/// Node in the reverse flow graph used to find the earliest
/// insertion point for stores which can potentially be folded.
struct ReverseNodeState {
  /// Originating nodes.
  DAGBlock &Node;
  /// Predecessor of the node.
  llvm::DenseSet<ReverseNodeState *> Succs;

  /// Set of stores which can be forwarded here.
  std::unordered_map
    < ID<Object>
    , std::map<uint64_t, std::pair<MemoryStoreInst *, uint64_t>>
    > Stores;
  /// Imprecise, tainted locations.
  BitSet<Object> Stored;

  /// Set of inaccurate loads.
  BitSet<Object> Loaded;

  ReverseNodeState(DAGBlock &node);

  /// LUB operator of two nodes.
  void Merge(const ReverseNodeState &that);

  /// @section Stores
  void Store(ID<Object> id);
  void Store(
      ID<Object> id,
      uint64_t start,
      uint64_t end,
      MemoryStoreInst *store = nullptr
  );
  void Store(const BitSet<Object> &changed);

  /// @section Loads
  void Load(ID<Object> id);
  void Load(ID<Object> id, uint64_t start, uint64_t end);
  void Load(const BitSet<Object> &loaded);

  /// Print information about the node to a stream.
  void dump(llvm::raw_ostream &os);
};
