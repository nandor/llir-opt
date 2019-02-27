// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cassert>
#include <cstdint>
#include <set>

#include <llvm/Support/raw_ostream.h>
/**
 * Sparse bit set implementation.
 */
template<typename T>
class BitSet final {
private:
  /// Node in the sparse bitset.
  struct Node {
    /// First 64 elements.
    uint64_t Fst;
    /// Last 64 elements.
    uint64_t Snd;
  };

public:
  /// Integer type identifying items.
  using Item = uint32_t;

  /// Iterator over the bitset items.
  class iterator final {
  public:
    iterator(const iterator &that)
      : it_(that.it_)
    {
    }

    iterator(std::set<uint32_t>::iterator it)
      : it_(it)
    {
    }

    Item operator * () const
    {
      return *it_;
    }

    iterator operator ++ ()
    {
      it_++;
      return *this;
    }

    bool operator != (const iterator &that) const
    {
      return it_ != that.it_;
    }

  private:
    /// Iterator over the hash map.
    std::set<uint32_t>::iterator it_;
  };

  /// Constructs a new bitset.
  explicit BitSet()
  {
  }

  /// Deletes the bitset.
  ~BitSet()
  {
  }

  /// Start iterator.
  iterator begin()
  {
    return iterator(items_.begin());
  }

  /// End iterator.
  iterator end()
  {
    return iterator(items_.end());
  }

  /// Inserts an item into the bitset.
  void Insert(Item item)
  {
    items_.insert(item);
  }

  /// Efficiently computes the union of two bitsets.
  bool Union(const BitSet &that)
  {
    bool changed = false;
    for (auto item : that.items_) {
      changed |= items_.insert(item).second;
    }
    return changed;
  }

  /// Checks if two bitsets are equal.
  bool operator == (const BitSet &that) const
  {
    return items_ == that.items_;
  }

  /// Disallow copy construct.
  BitSet(const BitSet &) = delete;
  /// Disallow assign.
  void operator = (const BitSet &) = delete;

private:
  /// Nodes stored in the bit set.
  std::set<uint32_t> items_;
};
