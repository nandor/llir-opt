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
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::MASK: {
      return TaggedType::Int();
    }
    case TaggedType::Kind::CONST: {
      return TaggedType::Int();
    }
    case TaggedType::Kind::ZERO_ONE:
    case TaggedType::Kind::INT: {
      return vr.IsUnknown() ? vr : TaggedType::Int();
    }
    case TaggedType::Kind::VAL: {
      return vr.IsUnknown() ? vr : TaggedType::Int();
    }
    case TaggedType::Kind::HEAP: {
      return vr.IsUnknown() ? vr : TaggedType::Int();
    }
    case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
    case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
    case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
    case TaggedType::Kind::UNDEF: return TaggedType::Undef();
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::PTR_NULL: return TaggedType::Int();
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
TaggedType Step::Shl(Type ty, TaggedType vl, TaggedType vr)
{
  if (vl.IsUnknown()) {
    return TaggedType::Unknown();
  }

  switch (vr.GetKind()) {
    case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
    case TaggedType::Kind::INT:       return TaggedType::Int();
    case TaggedType::Kind::PTR_INT:   return TaggedType::Int();
    case TaggedType::Kind::ZERO_ONE:  return TaggedType::Int();
    case TaggedType::Kind::MASK:      return TaggedType::Int();
    case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
    case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
    case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
    case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
    case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
    case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
    case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
    case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
    case TaggedType::Kind::CONST:     return TaggedType::Int();
  }
  llvm_unreachable("invalid value kind");
}
