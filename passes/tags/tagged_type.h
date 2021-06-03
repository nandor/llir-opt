// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/APInt.h>
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
    ONE,
    ZERO_ONE,
    CONST,
    MOD,
    INT,
    // Caml pointers.
    YOUNG,
    HEAP,
    TAG_PTR,
    ADDR,
    // Caml values.
    VAL,
    // Arbitrary pointers.
    PTR,
    PTR_NULL,
    PTR_INT,
    // Undefined/imprecise.
    UNDEF,
  };

  struct Mod {
    unsigned Div;
    unsigned Rem;
  };

public:
  TaggedType(const TaggedType &that);
  ~TaggedType();

  Kind GetKind() const { return k_; }

public:
  bool IsUnknown() const { return k_ == Kind::UNKNOWN; }
  bool IsZero() const { return k_ == Kind::ZERO; }
  bool IsOne() const { return k_ == Kind::ONE; }
  bool IsMod() const { return k_ == Kind::MOD; }
  bool IsVal() const { return k_ == Kind::VAL; }
  bool IsHeap() const { return k_ == Kind::HEAP; }
  bool IsInt() const { return k_ == Kind::INT; }
  bool IsPtrNull() const { return k_ == Kind::PTR_NULL; }
  bool IsPtrInt() const { return k_ == Kind::PTR_INT; }
  bool IsPtr() const { return k_ == Kind::PTR; }
  bool IsYoung() const { return k_ == Kind::YOUNG; }
  bool IsUndef() const { return k_ == Kind::UNDEF; }
  bool IsZeroOne() const { return k_ == Kind::ZERO_ONE; }

  bool IsEven() const;
  bool IsOdd() const;
  bool IsOddLike() const { return IsOdd() || IsOne(); }
  bool IsEvenLike() const { return IsEven() || IsZero(); }
  bool IsIntLike() const;
  bool IsPtrLike() const { return IsHeap() || IsPtr(); }
  bool IsPtrUnion() const { return IsVal() || IsPtrNull() || IsPtrInt(); }
  bool IsZeroOrOne() const { return IsZero() || IsOne(); }

  Mod GetMod() const { assert(IsMod()); return u_.ModVal; }

public:
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
  static TaggedType Zero() { return TaggedType(Kind::ZERO); }
  static TaggedType One() { return TaggedType(Kind::ONE); }
  static TaggedType Even() { return Modulo({ 2, 0 }); }
  static TaggedType Odd() { return Modulo({ 2, 1 }); }
  static TaggedType Modulo(const Mod &mod);
  static TaggedType ZeroOne() { return TaggedType(Kind::ZERO_ONE); }
  static TaggedType Int() { return TaggedType(Kind::INT); }
  static TaggedType Young() { return TaggedType(Kind::YOUNG); }
  static TaggedType Heap() { return TaggedType(Kind::HEAP); }
  static TaggedType Val() { return TaggedType(Kind::VAL); }
  static TaggedType Ptr() { return TaggedType(Kind::PTR); }
  static TaggedType PtrNull() { return TaggedType(Kind::PTR_NULL); }
  static TaggedType PtrInt() { return TaggedType(Kind::PTR_INT); }
  static TaggedType TagPtr() { return TaggedType(Kind::TAG_PTR); }
  static TaggedType Addr() { return TaggedType(Kind::ADDR); }
  static TaggedType Undef() { return TaggedType(Kind::UNDEF); }

private:
  TaggedType(Kind k) : k_(k) {}

  void Destroy();

  Kind k_;

  union U {
    Mod         ModVal;
    llvm::APInt IntVal;

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

// -----------------------------------------------------------------------------
inline unsigned GCD(unsigned a, unsigned b)
{
  while (b) {
    unsigned r = a % b;
    a = b;
    b = r;
  };
  return a;
}

// -----------------------------------------------------------------------------
inline unsigned LCM(unsigned a, unsigned b)
{
  return static_cast<unsigned long long>(a) * b / GCD(a, b);
}
