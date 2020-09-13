// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cassert>
#include <cstdint>
#include <climits>
#include <limits>
#include <map>

#include "core/adt/id.h"
#include <iostream>
#include <bitset>


/**
 * Sparse bit set implementation.
 */
template<typename T, unsigned N = 8>
class BitSet final {
private:
  /// Number of bits in a bucker.
  static constexpr uint64_t kBitsInBucket = sizeof(uint64_t) * CHAR_BIT;
  /// Number of bits in a chunk.
  static constexpr uint64_t kBitsInChunk = kBitsInBucket * N;

  /// Node in the sparse bitset.
  struct Node {
  public:
    Node()
    {
      for (unsigned i = 0; i < N; ++i) {
        arr[i] = 0ull;
      }
    }

    bool Insert(unsigned bit)
    {
      const uint64_t bucket = bit / kBitsInBucket;
      const uint64_t mask = 1ull << (bit - bucket * kBitsInBucket);
      const uint64_t val = arr[bucket];
      const bool inserted = !(val & mask);
      arr[bucket] = val | mask;
      return inserted;
    }

    bool Contains(unsigned bit) const
    {
      uint64_t bucket = bit / kBitsInBucket;
      const uint64_t mask = 1ull << (bit - bucket * kBitsInBucket);
      return arr[bucket] & mask;
    }

    bool Erase(unsigned bit)
    {
      const uint64_t bucket = bit / kBitsInBucket;

      arr[bucket] &= ~(1ull << (bit - bucket * kBitsInBucket));

      for (unsigned i = 0; i < N; ++i) {
        if (arr[i] != 0ull) {
          return false;
        }
      }
      return true;
    }

    bool operator==(const Node &that) const
    {
      for (unsigned i = 0; i < N; ++i) {
        if (arr[i] != that.arr[i]) {
          return false;
        }
      }
      return true;
    }

    unsigned Union(const Node &that)
    {
      unsigned changed = 0;
      for (unsigned i = 0; i < N; ++i) {
        uint64_t old = arr[i];
        arr[i] |= that.arr[i];
        changed += __builtin_popcountll(old ^ arr[i]);
      }
      return changed;
    }

  private:
    uint64_t arr[N];
  };

  /// Iterator over the structure holding the node.
  using NodeIt = typename std::map<uint32_t, Node>::const_iterator;

public:
  /// Iterator over the bitset items.
  class iterator final {
  public:
    iterator(const iterator &that)
      : set_(that.set_)
      , it_(that.it_)
      , current_(that.current_)
    {
    }

    iterator(const BitSet<T> &set, int64_t current)
      : set_(set)
      , it_(set.nodes_.find(current / kBitsInChunk))
      , current_(current)
    {
    }

    ID<T> operator * () const
    {
      return current_;
    }

    iterator operator ++ (int)
    {
      iterator it(*this);
      ++*this;
      return it;
    }

    iterator operator ++ ()
    {
      for (;;) {
        ++current_;
        if (current_ > set_.last_) {
          return *this;
        }

        uint64_t pos = current_ & (kBitsInChunk - 1);
        if (pos == 0) {
          ++it_;
          current_ = it_->first * kBitsInChunk;
        }

        if (it_->second.Contains(pos)) {
          return *this;
        }
      }

      return *this;
    }

    bool operator == (const iterator &that) const
    {
      return current_ == that.current_;
    }

    bool operator != (const iterator &that) const
    {
      return current_ != that.current_;
    }

  private:
    /// Reference to the set.
    const BitSet<T> &set_;
    /// Iterator over the hash map.
    NodeIt it_;
    /// Current item.
    uint64_t current_;
  };

  /// Reverse iterator over the bitset items.
  class reverse_iterator final {
  public:
    reverse_iterator(const reverse_iterator &that)
      : set_(that.set_)
      , it_(that.it_)
      , current_(that.current_)
    {
    }

    reverse_iterator(const BitSet<T> &set, int64_t current)
      : set_(set)
      , it_(set.nodes_.find(current / kBitsInChunk))
      , current_(current)
    {
    }

    ID<T> operator * () const
    {
      return current_;
    }

    reverse_iterator operator ++ ()
    {
      for (;;) {
        --current_;
        if (current_ < set_.first_) {
          return *this;
        }

        uint64_t pos = current_ & (kBitsInChunk - 1);
        if (pos == kBitsInChunk - 1) {
          --it_;
          current_ = (it_->first * kBitsInChunk) + kBitsInChunk - 1;
        }
        if (it_->second.Contains(pos)) {
          return *this;
        }
      }

      return *this;
    }

    bool operator != (const iterator &that) const
    {
      return current_ != that.current_;
    }

  private:
    /// Reference to the set.
    const BitSet<T> &set_;
    /// Iterator over the hash map.
    NodeIt it_;
    /// Current item.
    int64_t current_;
  };

  /// Constructs a new bitset.
  explicit BitSet()
    : first_(std::numeric_limits<uint32_t>::max())
    , last_(std::numeric_limits<uint32_t>::min())
  {
  }

  /// Deletes the bitset.
  ~BitSet()
  {
  }

  /// Start iterator.
  iterator begin() const
  {
    return Empty() ? end() : iterator(*this, first_);
  }

  /// End iterator.
  iterator end() const
  {
    return iterator(*this, static_cast<int64_t>(last_) + 1ull);
  }

  /// Reverse start iterator.
  reverse_iterator rbegin() const
  {
    return Empty() ? rend() : reverse_iterator(*this, last_);
  }

  /// Reverse end iterator.
  reverse_iterator rend() const
  {
    return reverse_iterator(*this, static_cast<int64_t>(first_) - 1ull);
  }

  /// Checks if the set is empty.
  bool Empty() const
  {
    return first_ > last_;
  }

  /// Clears these set.
  void Clear()
  {
    first_ = std::numeric_limits<uint32_t>::max();
    last_ = std::numeric_limits<uint32_t>::min();
    nodes_.clear();
  }

  /// Inserts an item into the bitset.
  bool Insert(const ID<T> &item)
  {
    first_ = std::min(first_, static_cast<uint32_t>(item));
    last_ = std::max(last_, static_cast<uint32_t>(item));

    auto &node = nodes_[item / kBitsInChunk];
    return node.Insert(item - (item / kBitsInChunk) * kBitsInChunk);
  }

  /// Erases a bit.
  void Erase(const ID<T> &item)
  {
    if (item == first_ && item == last_) {
      first_ = std::numeric_limits<uint32_t>::max();
      last_ = std::numeric_limits<uint32_t>::min();
    } else if (item == first_) {
      first_ = *++begin();
    } else if (item == last_) {
      last_ = *++rbegin();
    }

    auto &node = nodes_[item / kBitsInChunk];
    if (node.Erase(item - (item / kBitsInChunk) * kBitsInChunk)) {
      nodes_.erase(item / kBitsInChunk);
    }
  }

  /// Checks if a bit is set.
  bool Contains(const ID<T> &item) const
  {
    if (item < first_ || last_ < item) {
      return false;
    }
    auto it = nodes_.find(item / kBitsInChunk);
    if (it == nodes_.end()) {
      return false;
    }
    return it->second.Contains(item - (item / kBitsInChunk) * kBitsInChunk);
  }

  /// Efficiently computes the union of two bitsets.
  ///
  /// @return The number of newly set bits.
  unsigned Union(const BitSet &that)
  {
    unsigned changed = 0;

    for (auto &thatNode : that.nodes_) {
      changed += nodes_[thatNode.first].Union(thatNode.second);
    }

    first_ = std::min(first_, that.first_);
    last_ = std::max(last_, that.last_);

    return changed;
  }

  /// Subtracts a bitset from another.
  void Subtract(const BitSet &that) {
    for (auto elem : that) {
      Erase(elem);
    }
  }

  /// Checks if two bitsets are equal.
  bool operator == (const BitSet &that) const
  {
    if (first_ != that.first_) {
      return false;
    }
    if (last_ != that.last_) {
      return false;
    }

    if (nodes_.size() != that.nodes_.size()) {
      return false;
    }

    return std::equal(
        nodes_.begin(), nodes_.end(),
        that.nodes_.begin(), that.nodes_.end()
    );
  }

  /// Checks if two bitsets are different.
  bool operator != (const BitSet &that) const
  {
    return !operator==(that);
  }

private:
  /// First element.
  uint32_t first_;
  /// Last element.
  uint32_t last_;
  /// Nodes stored in the bit set.
  std::map<uint32_t, Node> nodes_;
};
