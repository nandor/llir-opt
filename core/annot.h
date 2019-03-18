// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <stdint.h>



/**
 * Allowed annotations.
 */
enum Annot {
  CAML_FRAME = 0,
  CAML_ROOT  = 1,
  CAML_VALUE = 3,
};


/**
 * Class representing a set of annotations.
 */
class AnnotSet final {
public:
  /// Iterator over annotations.
  class iterator final {
  public:
    /// Copies an iterator.
    iterator(const iterator &that) : mask_(that.mask_), idx_(that.idx_) {}

    /// Returns the annotation.
    Annot operator*() const { return static_cast<Annot>(idx_); }

    /// Moves to the next element (pre).
    iterator &operator++() { idx_ = Skip(idx_ + 1); return *this; }

    /// Moves to the next element (post).
    iterator operator++(int) { auto tmp = *this; ++*this; return tmp; }

    /// Checks if two iterators are equal.
    bool operator != (const iterator &that) const { return idx_ != that.idx_; }

  private:
    friend class AnnotSet;
    /// Creates a new iterator.
    iterator(uint64_t i, uint64_t mask) : mask_(mask), idx_(Skip(i)) {}

    /// Skips to the next set element.
    uint64_t Skip(uint64_t i)
    {
      while (i < sizeof(uint64_t) * 8 && (mask_ & (1 << i)) == 0) {
        ++i;
      }
      return i;
    }

  private:
    /// Annotation mask.
    uint64_t mask_;
    /// Current element.
    uint64_t idx_;
  };

public:
  /// Creats a new annotation set.
  AnnotSet() : annots_(0ull) {}

  /// Destroys the annotation set.
  ~AnnotSet();

  /// Checks if an annotation is set.
  bool Has(Annot annot) const
  {
    return annots_ & (1 << annot);
  }

  /// Sets an annotation.
  void Set(Annot annot)
  {
    annots_ |= 1 << annot;
  }

  /// Clears an annotation.
  void Clear(Annot annot)
  {
    annots_ &= ~(1 << annot);
  }

  /// Checks if any annotations are set.
  operator bool () const
  {
    return annots_ != 0;
  }

  /// Iterator to the first annotation.
  iterator begin() const { return iterator(0, annots_); }

  /// Iterator past the last annotation.
  iterator end() const { return iterator(sizeof(uint64_t) * 8, annots_); }

private:
  /// Mask indicating which annotations are set.
  uint64_t annots_;
};
