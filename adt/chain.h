// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <algorithm>



/**
 * Node in an intrusive list.
 */
template<typename T>
class ChainNode {
public:

private:
  /// Next node in the chain.
  T *Next;
  /// Previous node in the chain.
  T *Prev;
};

/**
 * Iterator over a chain.
 */
template<typename T>
class ChainIter : std::iterator<std::bidirectional_iterator_tag, T> {
private:
  using Traits = std::iterator<std::bidirectional_iterator_tag, T>;
  using reference = typename Traits::reference;

public:
  explicit ChainIter(T *elem)
  {
  }

  // Pre-increment.
  ChainIter& operator ++ ()
  {
    assert(!"not implemented");
  }

  // Post-increment.
  ChainIter operator ++ (int)
  {
    assert(!"not implemented");
  }

  // Pre-decrement.
  ChainIter& operator -- ()
  {
    assert(!"not implemented");
  }

  // Post-decrement.
  ChainIter operator -- (int)
  {
    assert(!"not implemented");
  }

  bool operator == (ChainIter other) const
  {
    assert(!"not implemented");
  }

  bool operator != (ChainIter other) const
  {
    assert(!"not implemented");
  }

  reference operator*() const
  {
    assert(!"not implemented");
  }

private:
  /// Current element iterated over.
  T *elem_;
};


/**
 * Chain of intrusive list nodes.
 */
template<typename T>
class Chain {
private:
  friend class ChainIter<T>;

public:
  using iterator = ChainIter<T>;
  using const_iterator = ChainIter<const T>;

  iterator begin() { return iterator(First); }
  iterator end() { return iterator(nullptr); }
  const_iterator begin() const { return const_iterator(First); }
  const_iterator end() const { return const_iterator(nullptr); }


private:
  /// First node in the chain.
  T *First;
};
