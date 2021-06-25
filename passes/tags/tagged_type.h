// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/APInt.h>
#include <llvm/Support/raw_ostream.h>

#include "passes/tags/masked_type.h"

namespace tags {

/**
 * Lattice of types, following a partial order.
 */
class TaggedType {
public:
  enum class Kind {
    UNKNOWN,
    // Integrals.
    INT,
    // Function pointer.
    FUNC,
    // Caml pointers.
    YOUNG,
    HEAP_OFF,
    HEAP,
    ADDR,
    ADDR_NULL,
    ADDR_INT,
    // Caml values.
    VAL,
    // Arbitrary pointers.
    PTR,
    PTR_NULL,
    PTR_INT,
    // Undefined/imprecise.
    UNDEF,
  };
public:
  TaggedType(const TaggedType &that);
  ~TaggedType();

  Kind GetKind() const { return k_; }

public:
  bool IsUnknown() const { return k_ == Kind::UNKNOWN; }
  bool IsInt() const { return k_ == Kind::INT; }
  bool IsYoung() const { return k_ == Kind::YOUNG; }
  bool IsHeap() const { return k_ == Kind::HEAP; }
  bool IsHeapOff() const { return k_ == Kind::HEAP_OFF; }
  bool IsPtrNull() const { return k_ == Kind::PTR_NULL; }
  bool IsPtrInt() const { return k_ == Kind::PTR_INT; }
  bool IsPtr() const { return k_ == Kind::PTR; }
  bool IsVal() const { return k_ == Kind::VAL; }
  bool IsUndef() const { return k_ == Kind::UNDEF; }
  bool IsAddr() const { return k_ == Kind::ADDR; }
  bool IsAddrInt() const { return k_ == Kind::ADDR_INT; }
  bool IsAddrNull() const { return k_ == Kind::ADDR_NULL; }
  bool IsFunc() const { return k_ == Kind::FUNC; }

  bool IsEven() const;
  bool IsOdd() const;
  bool IsZero() const;
  bool IsOne() const;
  bool IsOddLike() const { return IsOdd() || IsOne(); }
  bool IsEvenLike() const { return IsEven() || IsZero(); }
  bool IsPtrLike() const { return IsYoung() || IsHeap() || IsPtr() || IsAddr(); }
  bool IsPtrUnion() const;
  bool IsZeroOrOne() const;
  bool IsNonZero() const;

  MaskedType GetInt() const { assert(IsInt()); return u_.MaskVal; }

public:
  TaggedType ToPointer() const;

  TaggedType ToInteger() const;

  TaggedType &operator|=(const TaggedType &that)
  {
    *this = *this | that;
    return *this;
  }

  TaggedType operator|(const TaggedType &that) const;

  bool operator==(const TaggedType &that) const;

  bool operator!=(const TaggedType &that) const { return !(*this == that); }

  bool operator<(const TaggedType &that) const;

  bool operator<=(const TaggedType &that) const
  {
    return *this == that || *this < that;
  }

  TaggedType &operator=(const TaggedType &that);

  void dump(llvm::raw_ostream &os) const;

public:
  static TaggedType Unknown() { return TaggedType(Kind::UNKNOWN); }
  static TaggedType Young() { return TaggedType(Kind::YOUNG); }
  static TaggedType HeapOff() { return TaggedType(Kind::HEAP_OFF); }
  static TaggedType Heap() { return TaggedType(Kind::HEAP); }
  static TaggedType Val() { return TaggedType(Kind::VAL); }
  static TaggedType Ptr() { return TaggedType(Kind::PTR); }
  static TaggedType PtrNull() { return TaggedType(Kind::PTR_NULL); }
  static TaggedType PtrInt() { return TaggedType(Kind::PTR_INT); }
  static TaggedType TagPtr() { return TaggedType(Kind::ADDR_INT); }
  static TaggedType Addr() { return TaggedType(Kind::ADDR); }
  static TaggedType AddrNull() { return TaggedType(Kind::ADDR_NULL); }
  static TaggedType AddrInt() { return TaggedType(Kind::ADDR_INT); }
  static TaggedType Undef() { return TaggedType(Kind::UNDEF); }
  static TaggedType Func() { return TaggedType(Kind::FUNC); }

  static TaggedType Mask(const MaskedType &mod);
  static TaggedType Zero() { return Const(0); }
  static TaggedType One() { return Const(1); }
  static TaggedType Even() { return Mask({ 0, 1 }); }
  static TaggedType Odd() { return Mask({ 1, 1 }); }
  static TaggedType Int() { return Mask({ 0, 0 }); }

  static TaggedType Const(int64_t value)
  {
    return Mask({ static_cast<uint64_t>(value) });
  }

  static TaggedType ZeroOne()
  {
    return Mask({ 0ull, static_cast<uint64_t>(-1 & ~1) });
  }

private:
  TaggedType(Kind k) : k_(k) {}

  void Destroy();

  Kind k_;

  union U {
    MaskedType  MaskVal;
    U() {}
    ~U() {}
  } u_;
};

} // end namespace

// -----------------------------------------------------------------------------
inline llvm::raw_ostream &
operator<<(llvm::raw_ostream &os, const tags::TaggedType &t)
{
  t.dump(os);
  return os;
}
