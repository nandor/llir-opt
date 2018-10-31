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

  void SetNext(T *next) { next_ = next; }
  void SetPrev(T *prev) { prev_ = prev; }
  T* GetNext() const { return next_; }
  T* GetPrev() const { return prev_; }

private:
  /// Next node in the chain.
  T *next_;
  /// Previous node in the chain.
  T *prev_;
};


/**
 * Iterator over a chain.
 */
template<typename T, bool IsReverse>
class ChainIter {
public:
  explicit ChainIter(T *elem)
    : elem_(elem)
  {
  }

  // Pre-increment.
  ChainIter& operator ++ ()
  {
    elem_ = IsReverse ? elem_->GetPrev() : elem_->GetNext();
    return *this;
  }

  // Pre-decrement.
  ChainIter& operator -- ()
  {
    elem_ = IsReverse ? elem_->GetNext() : elem_->GetPrev();
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

  T *operator -> () const
  {
    return elem_;
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
public:
  using iterator = ChainIter<T, false>;
  using reverse_iterator = ChainIter<T, true>;
  using const_iterator = ChainIter<const T, false>;
  using const_reverse_iterator = ChainIter<const T, true>;

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
  reverse_iterator rbegin() { return reverse_iterator(last_); }
  reverse_iterator rend() { return reverse_iterator(nullptr); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(last_); }
  const_reverse_iterator rend() const { return const_reverse_iterator(nullptr); }

  /// Insert a node at the end of the list.
  void push_back(T& elem) { insert(end(), elem); }

  /// Insert a node before the iterator.
  iterator insert(iterator it, T& elem) {
    auto *next = &*it;
    auto *prev = next ? next->GetPrev() : last_;

    elem.SetPrev(prev);
    elem.SetNext(next);

    if (prev) {
      prev->SetNext(&elem);
    } else {
      first_ = &elem;
    }
    if (next) {
      next->SetPrev(&elem);
    } else {
      last_ = &elem;
    }

    return iterator(&elem);
  }

  /// Checks if the chain is empty.
  bool empty() const
  {
    return first_ == nullptr;
  }

  /// Returns the size of the chain.
  size_t size() const
  {
    size_t size = 0;
    for (T *node = first_; node != nullptr; node = node->SetNext()) {
      size++;
    }
    return size;
  }

private:
  /// First node in the chain.
  T *first_;
  /// Lasst node in the chain.
  T *last_;
};

