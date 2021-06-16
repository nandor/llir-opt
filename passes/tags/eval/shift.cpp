// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Shr(Type ty, TaggedType vl, TaggedType vr)
{
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
    case TaggedType::Kind::UNDEF:     return TaggedType::Undef();
    case TaggedType::Kind::ADDR:      return TaggedType::Int();
    case TaggedType::Kind::ADDR_INT:  return TaggedType::Int();
    case TaggedType::Kind::ADDR_NULL: return TaggedType::Int();
    case TaggedType::Kind::PTR:       return TaggedType::Int();
    case TaggedType::Kind::PTR_INT:   return TaggedType::Int();
    case TaggedType::Kind::PTR_NULL:  return TaggedType::Int();
    case TaggedType::Kind::FUNC:      return TaggedType::Int();
    case TaggedType::Kind::INT: {
      return TaggedType::Int();
    }
    case TaggedType::Kind::VAL: {
      return vr.IsUnknown() ? vr : TaggedType::Int();
    }
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP: {
      return vr.IsUnknown() ? vr : TaggedType::Int();
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
TaggedType Step::Shl(Type ty, TaggedType vl, TaggedType vr)
{
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return TaggedType::Unknown();
  }
  if (vr.IsNonZero()) {
    return TaggedType::Even();
  } else {
    return TaggedType::Int();
  }
}
