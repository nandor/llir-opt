// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/raw_ostream.h>


namespace tags {

enum class ConstraintType {
  BOT,
  // Pure integers.
  INT,
  // Pure pointers.
  PTR_BOT,
  YOUNG,
  HEAP,
  ADDR,
  PTR,
  FUNC,
  // Pointers or integers.
  ADDR_INT,
  PTR_INT,
  HEAP_INT,
};

ConstraintType LUB(ConstraintType a, ConstraintType b);
ConstraintType GLB(ConstraintType a, ConstraintType b);

}

// -----------------------------------------------------------------------------
bool operator<(tags::ConstraintType a, tags::ConstraintType b);

// -----------------------------------------------------------------------------
inline bool operator<=(tags::ConstraintType a, tags::ConstraintType b)
{
  return a == b || a < b;
}

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, tags::ConstraintType type);

