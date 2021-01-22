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

    size_t Size() const
    {
      size_t size = 0;
      for (unsigned i = 0; i < N; ++i) {
        size += __builtin_popcountll(arr[i]);
      }
      return size;
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

    bool Subtract(const Node &that)
    {
      bool zero = true;
      for (unsigned i = 0; i < N; ++i) {
        arr[i] = arr[i] & ~that.arr[i];
        zero = zero && (arr[i] == 0);
      }
      return zero;
    }

    bool And(const Node &that)
    {
      bool zero = true;
      for (unsigned i = 0; i < N; ++i) {
        arr[i] = arr[i] & that.arr[i];
        zero = zero && (arr[i] == 0);
      }
      return zero;
    }

    unsigned Next(unsigned bit) const
    {
      const unsigned bucket = bit / kBitsInBucket;
      const unsigned offset = bit - bucket * kBitsInBucket;

      uint64_t mask;
      if (offset + 1 == kBitsInBucket) {
        mask = 0;
      } else {
        mask = arr[bucket] & ~((1ull << (offset + 1)) - 1);
      }

      if (mask) {
        return bucket * kBitsInBucket + __builtin_ctzll(mask);
      } else {
        for (unsigned i = bucket + 1; i < N; ++i) {
          if (arr[i]) {
            return i * kBitsInBucket + __builtin_ctzll(arr[i]);
          }
        }
        return 0;
      }
    }

    unsigned First() const
    {
      for (unsigned i = 0; i < N; ++i) {
        if (arr[i]) {
          return i * kBitsInBucket + __builtin_ctzll(arr[i]);
        }
      }
      return 0;
    }

    unsigned Prev(unsigned bit) const
    {
      const unsigned bucket = bit / kBitsInBucket;
      const unsigned offset = bit - bucket * kBitsInBucket;

      uint64_t mask;
      if (offset == 0) {
        mask = 0;
      } else {
        mask = arr[bucket] & ((1ull << offset) - 1);
      }

      if (mask) {
        return (bucket + 1) * kBitsInBucket - __builtin_clzll(mask) - 1;
      } else {
        for (int i = bucket - 1; i >= 0; --i) {
          if (arr[i]) {
            return (i + 1) * kBitsInBucket - __builtin_clzll(arr[i]) - 1;
          }
        }
        return kBitsInChunk;
      }
    }

    unsigned Last() const
    {
      for (int i = N - 1; i >= 0; --i) {
        if (arr[i]) {
          return (i + 1) * kBitsInBucket - __builtin_clzll(arr[i]) - 1;
        }
      }
      return kBitsInChunk;
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
      : set_(&set)
      , it_(set.nodes_.find(current / kBitsInChunk))
      , current_(current)
    {
    }

    iterator &operator=(const iterator &that)
    {
      set_ = that.set_;
      it_ = that.it_;
      current_ = that.current_;
      return *this;
    }

    ID<T> operator*() const
    {
      return current_;
    }

    iterator operator++(int)
    {
      iterator it(*this);
      ++*this;
      return it;
    }

    iterator operator++()
    {
      if (current_ == set_->last_) {
        current_ = set_->last_ + 1;
      } else {
        unsigned currPos = current_ & (kBitsInChunk - 1);
        unsigned nextPos = it_->second.Next(currPos);
        if (nextPos == 0) {
          ++it_;
          current_ = it_->first * kBitsInChunk + it_->second.First();
        } else {
          current_ = it_->first * kBitsInChunk + nextPos;
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
      return !(*this == that);
    }

  private:
    /// Reference to the set.
    const BitSet<T> *set_;
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

    reverse_iterator operator ++ (int)
    {
      reverse_iterator it(*this);
      ++*this;
      return it;
    }

    reverse_iterator operator ++ ()
    {
      if (current_ == set_.first_) {
        current_ = set_.first_ - 1;
      } else {
        unsigned currPos = current_ & (kBitsInChunk - 1);
        unsigned nextPos = it_->second.Prev(currPos);
        if (nextPos == kBitsInChunk) {
          --it_;
          current_ = it_->first * kBitsInChunk + it_->second.Last();
        } else {
          current_ = it_->first * kBitsInChunk + nextPos;
        }
      }
      return *this;
    }

    bool operator==(const reverse_iterator &that) const
    {
      return current_ == that.current_;
    }

    bool operator!=(const reverse_iterator &that) const
    {
      return !(*this == that);
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

  /// Constructs a singleton bitset.
  explicit BitSet(ID<T> id)
    : BitSet()
  {
    Insert(id);
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
  bool Empty() const { return nodes_.empty(); }

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

  /// Computes the union of two bitsets.
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
  void Subtract(const BitSet &that)
  {
    auto it = nodes_.begin();
    auto tt = that.nodes_.begin();
    while (it != nodes_.end() && tt != that.nodes_.end()) {
      // Advance iterators until indices match.
      while (it != nodes_.end() && it->first < tt->first) {
        ++it;
      }
      if (it == nodes_.end()) {
        return;
      }
      while (tt != that.nodes_.end() && tt->first < it->first) {
        ++tt;
      }
      if (tt == that.nodes_.end()) {
        return;
      }

      // Erase the node if all bits are deleted.
      if (it->first == tt->first) {
        if (it->second.Subtract(tt->second)) {
          nodes_.erase(it++);
        } else {
          ++it;
        }
      }
    }

    ResetFirstLast();
  }

  /// Subtracts a bitset from another.
  void Intersect(const BitSet &that)
  {
    auto it = nodes_.begin();
    auto tt = that.nodes_.begin();
    while (it != nodes_.end() && tt != that.nodes_.end()) {
      // Advance iterators until indices match.
      while (it != nodes_.end() && it->first < tt->first) {
        nodes_.erase(it++);
      }
      if (it == nodes_.end()) {
        return;
      }
      while (tt != that.nodes_.end() && tt->first < it->first) {
        ++tt;
      }
      if (tt == that.nodes_.end()) {
        return;
      }

      // Erase the node if all bits are deleted.
      if (it->first == tt->first) {
        if (it->second.And(tt->second)) {
          nodes_.erase(it++);
        } else {
          ++it;
        }
        ++tt;
      }
    }

    ResetFirstLast();
  }

  /// Returns the size of the document.
  size_t Size() const
  {
    size_t size = 0;
    for (auto &[id, node] : nodes_) {
      size += node.Size();
    }
    return size;
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

  /// Subtraction.
  BitSet operator-(const BitSet &that) const
  {
    BitSet copy(*this);
    copy.Subtract(that);
    return copy;
  }

  /// Bitwise or.
  BitSet operator|(const BitSet &that) const
  {
    BitSet copy(*this);
    copy.Union(that);
    return copy;
  }

  /// Bitwise or.
  BitSet &operator|=(const BitSet &that)
  {
    Union(that);
    return *this;
  }

  /// Bitwise and.
  BitSet operator&(const BitSet &that) const
  {
    BitSet copy(*this);
    copy.Intersect(that);
    return copy;
  }

private:
  /// Recompute the cached values of first and last.
  void ResetFirstLast()
  {
    if (nodes_.empty()) {
      first_ = std::numeric_limits<uint32_t>::max();
      last_ = std::numeric_limits<uint32_t>::min();
    } else {
      auto &[fi, fo] = *nodes_.begin();
      auto &[li, lo] = *nodes_.rbegin();
      first_ = fi * kBitsInChunk + fo.First();
      last_ = li * kBitsInChunk + lo.Last();
    }
  }

private:
  /// First element.
  uint32_t first_;
  /// Last element.
  uint32_t last_;
  /// Nodes stored in the bit set.
  std::map<uint32_t, Node> nodes_;
};

/// Print the bitset to a stream.
template <typename T>
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const BitSet<T> &s)
{
  os << "{";
  bool first = true;
  for (auto id : s) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << id;
  }
  os << "}";
  return os;
}
