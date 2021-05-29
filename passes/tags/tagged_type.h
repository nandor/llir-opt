// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/raw_ostream.h>



namespace tags {

/**
 * Lattice of types, following a partial order.
 */
class TaggedType {
public:
  enum class Kind {
    UNKNOWN,
    // Integrals.
    ZERO,
    EVEN,
    ONE,
    ODD,
    ZERO_ONE,
    INT,
    // Caml pointers.
    YOUNG,
    HEAP,
    // Caml values.
    VAL,
    // Regular pointers.
    PTR,
    PTR_NULL,
    PTR_INT,
    // Undefined/imprecise.
    UNDEF,
    ANY,
  };

  Kind GetKind() const { return k_; }

  bool IsUnknown() const { return k_ == Kind::UNKNOWN; }
  bool IsZero() const { return k_ == Kind::ZERO; }
  bool IsEven() const { return k_ == Kind::EVEN; }
  bool IsOne() const { return k_ == Kind::ONE; }
  bool IsOdd() const { return k_ == Kind::ODD; }
  bool IsAny() const { return k_ == Kind::ANY; }
  bool IsVal() const { return k_ == Kind::VAL; }
  bool IsHeap() const { return k_ == Kind::HEAP; }
  bool IsInt() const { return k_ == Kind::INT; }
  bool IsPtrNull() const { return k_ == Kind::PTR_NULL; }
  bool IsPtrInt() const { return k_ == Kind::PTR_INT; }
  bool IsPtr() const { return k_ == Kind::PTR; }
  bool IsYoung() const { return k_ == Kind::YOUNG; }
  bool IsUndef() const { return k_ == Kind::UNDEF; }
  bool IsZeroOne() const { return k_ == Kind::ZERO_ONE; }

  bool IsOddLike() const { return IsOdd() || IsOne(); }
  bool IsEvenLike() const { return IsEven() || IsZero(); }
  bool IsIntLike() const;
  bool IsPtrLike() const { return IsHeap() || IsPtr(); }
  bool IsPtrUnion() const { return IsVal() || IsPtrNull() || IsPtrInt(); }
  bool IsZeroOrOne() const { return IsZero() || IsOne(); }

  TaggedType &operator|=(const TaggedType &that);

  TaggedType operator|(const TaggedType &that) const
  {
    TaggedType r = *this;
    r |= that;
    return r;
  }

  bool operator==(const TaggedType &that) const;

  bool operator!=(const TaggedType &that) const { return !(*this == that); }

  bool operator<(const TaggedType &that) const;

  bool operator<=(const TaggedType &that) const
  {
    return *this == that || *this < that;
  }

  static TaggedType Unknown() { return TaggedType(Kind::UNKNOWN); }
  static TaggedType Zero() { return TaggedType(Kind::ZERO); }
  static TaggedType Even() { return TaggedType(Kind::EVEN); }
  static TaggedType One() { return TaggedType(Kind::ONE); }
  static TaggedType Odd() { return TaggedType(Kind::ODD); }
  static TaggedType ZeroOne() { return TaggedType(Kind::ZERO_ONE); }
  static TaggedType Int() { return TaggedType(Kind::INT); }
  static TaggedType Young() { return TaggedType(Kind::YOUNG); }
  static TaggedType Heap() { return TaggedType(Kind::HEAP); }
  static TaggedType Val() { return TaggedType(Kind::VAL); }
  static TaggedType Ptr() { return TaggedType(Kind::PTR); }
  static TaggedType PtrNull() { return TaggedType(Kind::PTR_NULL); }
  static TaggedType PtrInt() { return TaggedType(Kind::PTR_INT); }
  static TaggedType Undef() { return TaggedType(Kind::UNDEF); }
  static TaggedType Any() { return TaggedType(Kind::ANY); }

  void dump(llvm::raw_ostream &os) const;

private:
  TaggedType(Kind k) : k_(k) {}

  Kind k_;
};

} // end namespace

// -----------------------------------------------------------------------------
inline llvm::raw_ostream &
operator<<(llvm::raw_ostream &os, const tags::TaggedType &t)
{
  t.dump(os);
  return os;
}
