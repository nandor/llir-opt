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
    EVEN,
    ODD,
    INT,
    PTR,
    YOUNG,
    VAL,
    HEAP,
    ONE,
    ZERO,
    ZERO_OR_ONE,
    INT_OR_PTR,
    UNDEF,
    ANY,
  };

  Kind GetKind() const { return k_; }

  bool IsUnknown() const { return k_ == Kind::UNKNOWN; }
  bool IsAny() const { return k_ == Kind::ANY; }
  bool IsEven() const { return k_ == Kind::EVEN; }
  bool IsOdd() const { return k_ == Kind::ODD; }
  bool IsVal() const { return k_ == Kind::VAL; }
  bool IsInt() const { return k_ == Kind::INT; }
  bool IsIntOrPtr() const { return k_ == Kind::INT_OR_PTR; }
  bool IsZero() const { return k_ == Kind::ZERO; }
  bool IsOne() const { return k_ == Kind::ONE; }
  bool IsPtr() const { return k_ == Kind::PTR; }
  bool IsYoung() const { return k_ == Kind::YOUNG; }
  bool IsUndef() const { return k_ == Kind::UNDEF; }

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
  static TaggedType Any() { return TaggedType(Kind::ANY); }
  static TaggedType Int() { return TaggedType(Kind::INT); }
  static TaggedType Ptr() { return TaggedType(Kind::PTR); }
  static TaggedType Val() { return TaggedType(Kind::VAL); }
  static TaggedType Odd() { return TaggedType(Kind::ODD); }
  static TaggedType One() { return TaggedType(Kind::ONE); }
  static TaggedType Young() { return TaggedType(Kind::YOUNG); }
  static TaggedType Zero() { return TaggedType(Kind::ZERO); }
  static TaggedType ZeroOrOne() { return TaggedType(Kind::ZERO_OR_ONE); }
  static TaggedType Even() { return TaggedType(Kind::EVEN); }
  static TaggedType Heap() { return TaggedType(Kind::HEAP); }
  static TaggedType Undef() { return TaggedType(Kind::UNDEF); }
  static TaggedType IntOrPtr() { return TaggedType(Kind::INT_OR_PTR); }

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
