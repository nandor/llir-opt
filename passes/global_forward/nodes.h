// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/adt/bitset.h"
#include "core/dag.h"
#include "core/type.h"
#include "core/ref.h"

class Object;
class Inst;
class MemoryStoreInst;



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

  void Overwrite(ID<Object> changed);
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
    > StorePrecise;
  /// Imprecise, tainted locations.
  BitSet<Object> StoreImprecise;

  /// Set of accurate loads.
  std::unordered_map
    < ID<Object>
    , std::set<std::pair<uint64_t, uint64_t>>
    > LoadPrecise;
  /// Set of inaccurate loads.
  BitSet<Object> LoadImprecise;

  ReverseNodeState(DAGBlock &node);

  void Merge(const ReverseNodeState &that);

  /// @section Stores
  void Store(ID<Object> id);
  void Store(
      ID<Object> id,
      uint64_t start,
      uint64_t end,
      MemoryStoreInst *store = nullptr
  );
  void Store(const BitSet<Object> &stored);

  /// @section Loads
  void Load(ID<Object> id);
  void Load(ID<Object> id, uint64_t start, uint64_t end);
  void Load(const BitSet<Object> &loaded);

  /// Over-approximates the whole set.
  void Taint(const BitSet<Object> &changed);

  void dump(llvm::raw_ostream &os);
};
