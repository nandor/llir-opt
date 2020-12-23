// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>
#include <memory>

#include "core/adt/id.h"

class Node;
class DerefNode;
class GraphNode;
class RootNode;
class SetNode;



/**
 * Class storing and representing the graph.
 */
class Graph final {
public:
  /**
   * Iterator over the sets.
   */
  class SetIterator : public std::iterator<std::forward_iterator_tag, SetNode *> {
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

    SetNode *operator*() const { return *it_; }

  private:
    friend class Graph;

    SetIterator(
        Graph *graph,
        std::vector<SetNode *>::iterator it)
      : graph_(graph)
      , it_(it)
    {
      Skip();
    }

    void Skip()
    {
      while (it_ != graph_->sets_.end() && !*it_) {
        ++it_;
      }
    }

  private:
    /// Parent graph.
    Graph *graph_;
    /// Underlying iterator.
    std::vector<SetNode *>::iterator it_;
  };

public:
  /// Creates a set node.
  SetNode *Set();
  /// Creates a deref node.
  DerefNode *Deref(SetNode *set);
  /// Creates a root node.
  RootNode *Root(SetNode *set);

  /// Returns a set mapped to an id.
  SetNode *Get(ID<SetNode *> id) const { return sets_[id]; }
  /// Returns a deref mapped to an id.
  DerefNode *Get(ID<DerefNode *> id) const { return derefs_[id]; }

  /// Finds a set node, given its ID.
  SetNode *Find(ID<SetNode *> id);
  /// Finds a deref node, given its ID.
  DerefNode *Find(ID<DerefNode *> id) const { return derefs_[id]; }

  /// Unifies two nodes.
  SetNode *Union(SetNode *a, SetNode *b);

  /// Iterator over sets - begin.
  SetIterator begin() { return SetIterator(this, sets_.begin()); }
  /// Iterator over sets - end.
  SetIterator end() { return SetIterator(this, sets_.end()); }

private:
  /// Replaces a set node with another.
  void Replace(SetNode *a, SetNode *b);
  /// Replaces a deref node with another.
  void Replace(DerefNode *a, DerefNode *b);

private:
  friend class SCCSolver;

  /// List of all set nodes.
  std::vector<SetNode *> sets_;
  /// List of all deref nodes.
  std::vector<DerefNode *> derefs_;
  /// List of root nodes.
  std::vector<RootNode *> roots_;

  /// All allocated nodes.
  std::vector<std::unique_ptr<Node>> nodes_;

  /// Union-Find information.
  struct Entry {
    /// Parent node ID.
    uint32_t Parent;
    /// Node rank.
    uint32_t Rank;

    /// Creates a new entry.
    Entry(uint32_t parent, uint32_t rank) : Parent(parent), Rank(rank) {}
  };

  /// Union-Find nodes.
  std::vector<Entry> unions_;
};
