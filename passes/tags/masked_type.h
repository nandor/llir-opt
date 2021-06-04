// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <stdint.h>

#include <llvm/Support/raw_ostream.h>



namespace tags {

/**
 * Class representing a masked type.
 */
class MaskedType {
public:
  MaskedType(uint64_t value);
  MaskedType(uint64_t value, uint64_t known);

  MaskedType operator+(const MaskedType &that) const;
  MaskedType operator-(const MaskedType &that) const;
  MaskedType operator&(const MaskedType &that) const;
  MaskedType operator|(const MaskedType &that) const;
  MaskedType operator^(const MaskedType &that) const;
  MaskedType operator~() const;

  bool operator==(const MaskedType &that) const
  {
    return value_ == that.value_ && known_ == that.known_;
  }

  void dump(llvm::raw_ostream &os) const;

  uint64_t GetValue() const { return value_; }
  uint64_t GetKnown() const { return known_; }

private:
  uint64_t value_;
  uint64_t known_;
};

}

// ----------------------------------------------------------------------------
inline llvm::raw_ostream &
operator<<(llvm::raw_ostream &os, const tags::MaskedType &m)
{
  m.dump(os);
  return os;
}

