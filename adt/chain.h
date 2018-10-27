// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once



/**
 * Node in an intrusive list.
 */
template<typename T>
class ChainNode {
public:
  /// Creates a new chain node.
  ChainNode()
    : next_(nullptr)
    , prev_(nullptr)
  {
  }

  void setNext(T *next) { next_ = next; }
  void setPrev(T *prev) { prev_ = prev; }
  T* getNext() const { return next_; }
  T* getPrev() const { return prev_; }

private:
  /// Next node in the chain.
  T *next_;
  /// Previous node in the chain.
  T *prev_;
};

/**
 * Iterator over a chain.
 */
template<typename T>
class ChainIter {
public:
  explicit ChainIter(T *elem)
    : elem_(elem)
  {
  }

  // Pre-increment.
  ChainIter& operator ++ ()
  {
    elem_ = elem_->getNext();
    return *this;
  }

  // Pre-decrement.
  ChainIter& operator -- ()
  {
    elem_ = elem_->getPrev();
    return *this;
  }

  // Post-increment.
  ChainIter operator ++ (int)
  {
    ChainIter it = *this;
    ++*this;
    return it;
  }

  // Post-decrement.
  ChainIter operator -- (int)
  {
    ChainIter it = *this;
    --*this;
    return it;
  }

  bool operator == (ChainIter that) const
  {
    return elem_ == that.elem_;
  }

  bool operator != (ChainIter that) const
  {
    return !(*this == that);
  }

  T &operator * () const
  {
    return *elem_;
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

  /// Creates a new chain.
  Chain()
    : first_(nullptr)
    , last_(nullptr)
  {
  }

  // Iterators over the chain.
  iterator begin() { return iterator(first_); }
  iterator end() { return iterator(nullptr); }
  const_iterator begin() const { return const_iterator(first_); }
  const_iterator end() const { return const_iterator(nullptr); }

  /// Insert a node at the end of the list.
  void push_back(T& elem) { insert(end(), elem); }

  /// Insert a node before the iterator.
  iterator insert(iterator it, T& elem) {
    auto *next = &*it;
    auto *prev = next ? next->getPrev() : last_;

    elem.setPrev(prev);
    elem.setNext(next);

    if (prev) {
      prev->setNext(&elem);
    } else {
      first_ = &elem;
    }
    if (next) {
      next->setPrev(&elem);
    } else {
      last_ = &elem;
    }

    return iterator(&elem);
  }

private:
  /// First node in the chain.
  T *first_;
  /// Lasst node in the chain.
  T *last_;
};
