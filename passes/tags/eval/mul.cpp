// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Mul(TaggedType vl, TaggedType vr)
{
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return TaggedType::Unknown();
  }
  if (vl.IsZero() || vr.IsZero()) {
    return TaggedType::Zero();
  }
  if (vl.IsOne() || vr.IsOne()) {
    auto other = vl.IsOne() ? vr : vl;
    switch (other.GetKind()) {
      case TaggedType::Kind::UNKNOWN: {
        return TaggedType::Unknown();
      }
      case TaggedType::Kind::INT: {
        return other;
      }
      case TaggedType::Kind::YOUNG:
      case TaggedType::Kind::HEAP:
      case TaggedType::Kind::HEAP_OFF:
      case TaggedType::Kind::VAL:
      case TaggedType::Kind::PTR:
      case TaggedType::Kind::PTR_INT:
      case TaggedType::Kind::PTR_NULL:
      case TaggedType::Kind::ADDR:
      case TaggedType::Kind::ADDR_INT:
      case TaggedType::Kind::ADDR_NULL: {
        return TaggedType::Int();
      }
      case TaggedType::Kind::UNDEF: {
        return TaggedType::Undef();
      }
    }
    llvm_unreachable("invalid kind");
  }
  if (vl.IsEven() || vr.IsEven()) {
    return TaggedType::Even();
  }
  if (vl.IsOdd() && vr.IsOdd()) {
    return TaggedType::Odd();
  }
  return TaggedType::Int();
}
