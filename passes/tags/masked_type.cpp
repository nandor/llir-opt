// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/MathExtras.h>

#include "passes/tags/masked_type.h"

using namespace tags;



// -----------------------------------------------------------------------------
MaskedType::MaskedType(uint64_t value)
  : value_(value)
  , known_(-1ll)
{
}

// -----------------------------------------------------------------------------
MaskedType::MaskedType(uint64_t value, uint64_t known)
  : value_(value & known)
  , known_(known)
{
}

// -----------------------------------------------------------------------------
MaskedType MaskedType::operator+(const MaskedType &that) const
{
  uint64_t sum = value_ + that.value_;
  uint64_t t = llvm::countTrailingZeros(~(known_ & that.known_));
  uint64_t mask = t == 64 ? static_cast<uint64_t>(-1) : ((1ull << t) - 1);
  return MaskedType(sum, mask);
}

// -----------------------------------------------------------------------------
MaskedType MaskedType::operator-(const MaskedType &that) const
{
  return *this + ~that + MaskedType(1);
}

// -----------------------------------------------------------------------------
MaskedType MaskedType::operator&(const MaskedType &that) const
{
  auto zeros = ~(value_ & that.value_) & (known_ & that.known_);
  auto known = zeros | (known_ & that.known_);
  return MaskedType(value_ & that.value_ & known, known);
}

// -----------------------------------------------------------------------------
MaskedType MaskedType::operator|(const MaskedType &that) const
{
  auto known = (known_ & that.known_) | value_ | that.value_;
  auto value = value_ | that.value_;
  return MaskedType(value, known);
}

// -----------------------------------------------------------------------------
MaskedType MaskedType::operator^(const MaskedType &that) const
{
  auto known = known_ & that.known_;
  auto value = value_ ^ that.value_;
  return MaskedType(value, known);
}

// -----------------------------------------------------------------------------
MaskedType MaskedType::operator~() const
{
  return MaskedType(~value_, known_);
}

// -----------------------------------------------------------------------------
void MaskedType::dump(llvm::raw_ostream &os) const
{
  for (unsigned i = 0, n = CHAR_BIT * sizeof(uint64_t); i < n; ++i) {
    uint64_t bit = (1ull << static_cast<uint64_t>(n - i - 1));
    if (known_ & bit) {
      os << ((value_ & bit) != 0);
    } else {
      os << "x";
    }
  }
}
