// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include "core/adt/id.h"

#include <iostream>

/**
 * Union-find data structure to manage nodes.
 */
template <typename T>
class UnionFind {
public:
  UnionFind() : size_(0) {}

  template <typename... Args>
  ID<T> Emplace(Args... args)
  {
    size_++;
    size_t n = entries_.size();
    ID<T> id(n);
    entries_.emplace_back(
        n,
        std::make_unique<T>(id, std::forward<Args>(args)...)
    );
    return id;
  }

  ID<T> Union(ID<T> idA, ID<T> idB)
  {
    size_t idxA = Find(idA);
    size_t idxB = Find(idB);
    if (idxA == idxB) {
      return idxB;
    }

    size_--;

    Entry &entryA = entries_[idxA];
    Entry &entryB = entries_[idxB];
    T *a = entryA.Element.get();
    T *b = entryB.Element.get();

    if (entryA.Rank < entryB.Rank) {
      entryA.Parent = idB;
      b->Union(*a);
      entries_[idA].Element = nullptr;
      return idB;
    } else {
      entryB.Parent = idA;
      a->Union(*b);
      entries_[idB].Element = nullptr;
      if (entryA.Rank == entryB.Rank) {
        entryA.Rank += 1;
      }
      return idA;
    }
  }

  T *Map(ID<T> id) const
  {
    return entries_[Find(id)].Element.get();
  }

  ID<T> Find(ID<T> id) const
  {
    size_t root = id;
    while (entries_[root].Parent != root) {
      root = entries_[root].Parent;
    }
    while (entries_[id].Parent != id) {
      size_t parent = entries_[id].Parent;
      entries_[id].Parent = root;
      id = parent;
    }
    return id;
  }


  size_t Size() const { return size_; }

private:
  /// Union-Find entry.
  struct Entry {
    /// Parent ID.
    mutable size_t Parent;
    /// Union-Find Rank.
    mutable size_t Rank;
    /// Element or nullptr if unified.
    std::unique_ptr<T> Element;

    Entry(size_t n, std::unique_ptr<T> &&element)
      : Parent(n)
      , Rank(0)
      , Element(std::move(element))
    {
    }
  };

  /// Mapping from indices to entries.
  std::vector<Entry> entries_;
  /// Number of distinct nodes.
  size_t size_;
};
