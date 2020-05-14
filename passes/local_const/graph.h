// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/iterator.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/raw_ostream.h>

#include "core/adt/id.h"
#include "core/adt/bitset.h"
#include "passes/local_const/scc.h"



/// Forward declaration of the graph and its elements.
class LCAlloc;
class LCSet;
class LCDeref;
class LCSCC;


/**
 * Container for the whole graph.
 */
class LCGraph {
private:
  /// Union-Find information.
  struct Entry {
    /// Parent node ID.
    uint32_t Parent;
    /// Node rank.
    uint32_t Rank;
    /// Set or nullptr if united.
    std::unique_ptr<LCSet> Set;

    /// Creates a new entry.
    Entry(LCGraph *g, uint32_t id);
  };

public:
  /**
   * Iterator over the uses.
   */
  class SetIterator : public std::iterator<std::forward_iterator_tag, LCSet *> {
  public:
    bool operator==(const SetIterator &x) const { return it_ == x.it_; }
    bool operator!=(const SetIterator &x) const { return !operator==(x); }

    SetIterator &operator++() {
      ++it_;
      Skip();
      return *this;
    }

    SetIterator operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    LCSet *operator*() const { return it_->Set.get(); }

  private:
    friend class LCGraph;

    SetIterator(
        LCGraph *graph,
        std::vector<Entry>::iterator it)
      : graph_(graph)
      , it_(it)
    {
      Skip();
    }

    void Skip()
    {
      while (it_ != graph_->sets_.end() && !it_->Set.get()) {
        ++it_;
      }
    }

  private:
    /// Parent graph.
    LCGraph *graph_;
    /// Underlying iterator.
    std::vector<Entry>::iterator it_;
  };

public:
  /// Creates a new set.
  LCSet *Set();
  /// Creates a new allocation site.
  LCAlloc *Alloc(const std::optional<uint64_t> &size, uint64_t maxSize);

  /// Returns a set attached to an ID, null if unified.
  LCSet *Get(ID<LCSet> id);
  /// Returns a set attached or unified to the ID.
  LCSet *Find(ID<LCSet> id);
  /// Returns a deref attached to the ID.
  LCDeref *Find(ID<LCDeref> id);
  /// Returns an allocation attached or unified with the ID.
  LCAlloc *Find(ID<LCAlloc> id);

  /// Unifies two nodes.
  ID<LCSet> Union(ID<LCSet> a, ID<LCSet> b);

  /// Iterator over sets - begin.
  SetIterator begin() { return SetIterator(this, sets_.begin()); }
  /// Iterator over sets - end.
  SetIterator end() { return SetIterator(this, sets_.end()); }

private:
  void Replace(LCSet *a, LCSet *b);
  void Replace(LCDeref *a, LCDeref *b);

private:
  friend class LCSCC;
  /// LCAlloc site trackers.
  std::vector<std::unique_ptr<LCAlloc>> allocs_;
  /// LCSet trackers.
  std::vector<Entry> sets_;
};


/**
 * Structure representing an offset.
 */
class LCIndex {
public:
  /// Out-of-bounds after the object into modelled region: access allowed.
  constexpr static uint64_t kPositive = static_cast<uint64_t>(-2);
  /// Invalid index in an object of known size.
  constexpr static uint64_t kInvalid = static_cast<uint64_t>(-3);

public:
  /// Returns the numeric offset.
  operator uint64_t () const { return index_; }

  /// Checks if the index points to an actual field.
  bool IsField() const { return index_ != kInvalid && index_ != kPositive; }

private:
  explicit LCIndex(uint64_t index) : index_(index) {}

private:
  friend class LCAlloc;
  friend class LCSet;
  uint64_t index_;
};

/**
 * A class representing an allocation site.
 *
 * The site is formed of two special nodes, in and out, along with elem nodes.
 *
 *      I
 *    / | \
 *   /  |  \
 *  -1  0  +1 ...
 *   \  |  /
 *    \ | /
 *      O
 */
class LCAlloc {
public:
  /// Returns the allocation ID.
  ID<LCAlloc> GetID() const { return id_; }

  /// Creates a new offset into the allocation site.
  std::optional<LCIndex> Offset(LCIndex index, int64_t offset);

  /// Returns the set for a given element.
  std::optional<ID<LCSet>> GetElement(LCIndex index);

  /// Creates an index into this object.
  LCIndex GetIndex(uint64_t Index);

  /// Returns the incoming range node.
  ID<LCSet> GetNodeIn() { return nodeIn_; }

  /// Returns the outgoing range node.
  ID<LCSet> GetNodeOut() { return nodeOut_; }

private:
  /// Creates an allocation site of known size.
  LCAlloc(
      LCGraph *graph,
      ID<LCAlloc> id,
      const std::optional<uint64_t> &size,
      uint64_t maxSize
  );

private:
  friend class LCGraph;
  /// Reference to the graph.
  LCGraph *graph_;
  /// Identifier of the allocation.
  ID<LCAlloc> id_;
  /// Size of the object, if known.
  std::optional<uint64_t> allocSize_;
  /// Size of the modelled section of the object.
  uint64_t size_;
  /// Incoming node - used to represent writes to a range.
  ID<LCSet> nodeIn_;
  /// Outgoing node - used to represent a read from a range.
  ID<LCSet> nodeOut_;
  /// Optional node for out-of-bounds storage.
  ID<LCSet> bucket_;
  /// Sets of individual elements.
  llvm::DenseMap<uint64_t, ID<LCSet>> elems_;
};


/**
 * A class representing a set of pointers.
 */
class LCSet final : public LCNode {
public:
  /// Returns the node ID.
  ID<LCSet> GetID() const { return id_; }

  /// Deref node attached to the set.
  LCDeref *Deref();
  /// Returns the deref node, if it exists.
  LCDeref *GetDeref() const { return deref_.get(); }

  /// Adds an element to the set.
  bool AddElement(LCAlloc *alloc, LCIndex elem);
  /// Adds a whole range to the set.
  bool AddRange(LCAlloc *alloc);

  /// Adds an edge to a set.
  bool Edge(LCSet *set);
  /// Adds an edge to a deref.
  bool Edge(LCDeref *deref);
  /// Adds a range edge.
  bool Range(LCSet *set);
  /// Adds an offset edge.
  bool Offset(LCSet *set, int64_t offset);

  /// Propagates values to another set.
  bool Propagate(LCSet *to);

  /// Checks if two sets are equal.
  bool Equals(LCSet *that);

  /// Iterator over the outgoing sets.
  void sets(std::function<void(LCSet *)> &&f);
  /// Iterator over the outgoing ranges.
  void ranges(std::function<void(LCSet *)> &&f);
  /// Iterator over the outgoing offsets.
  void offsets(std::function<void(LCSet *, int64_t)> &&f);

  /// Iterator over the outgoing derefs.
  void deref_outs(std::function<void(LCDeref *)> &&f);

  /// Iterator over the sets in a set.
  void points_to_set(std::function<void(ID<LCSet>, ID<LCSet>)> &&f);
  /// Iterator over the full objects in a set.
  void points_to_range(std::function<void(LCAlloc *)> &&f);
  /// Iterator over the elements in a set.
  void points_to_elem(std::function<void(LCAlloc *, LCIndex)> &&f);

  /// Dumps the set.
  void dump(llvm::raw_ostream &os = llvm::errs());

private:
  /// Creates a set node.
  LCSet(LCGraph *graph, ID<LCSet> id) : graph_(graph), id_(id) {}

private:
  friend class LCGraph;
  friend class LCDeref;

  /// Reference to the graph.
  LCGraph *graph_;
  /// The ID of the node.
  ID<LCSet> id_;
  /// Deref node associated with the set.
  std::unique_ptr<LCDeref> deref_;

  /// Outgoing sets.
  BitSet<LCSet> setOuts_;
  /// Outgoing ranges.
  BitSet<LCSet> rangeOuts_;
  /// Incoming deref nodes - loads from pointers.
  BitSet<LCSet> derefIns_;
  /// Outgoing deref nodes - stores to pointers.
  BitSet<LCSet> derefOuts_;
  /// Outgoing offset nodes.
  std::set<std::pair<ID<LCSet>, int64_t>> offsetOuts_;

  /// Points-to ranges.
  BitSet<LCAlloc> pointsToRange_;
  /// Element sets.
  std::set<std::pair<ID<LCAlloc>, uint64_t>> pointsToElem_;
};


/**
 * A class dereferencing a set of pointers.
 */
class LCDeref final : public LCNode {
public:
  /// Adds an edge to a set.
  bool Edge(LCSet *set);

  /// The deref node can be found using the set ID.
  ID<LCDeref> GetID() const { return static_cast<uint32_t>(set_->GetID()); }

  /// Incoming store edges.
  void set_ins(std::function<void(LCSet *)> &&f);

  /// Outgoing load edges.
  void set_outs(std::function<void(LCSet *)> &&f);

private:
  LCDeref(LCGraph *graph, LCSet *set) : graph_(graph), set_(set) { }

private:
  friend class LCSet;
  friend class LCGraph;

  /// Reference to the graph.
  LCGraph *graph_;
  /// Set which owns this dereference node.
  LCSet *set_;

  /// Incoming sets - stores.
  BitSet<LCSet> setIns_;
  /// Outgoing sets - loads.
  BitSet<LCSet> setOuts_;
};

