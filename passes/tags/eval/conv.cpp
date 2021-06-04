// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Ext(Type ty, TaggedType arg)
{
  switch (arg.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::CONST:
    case TaggedType::Kind::MASK:
    case TaggedType::Kind::INT:
    case TaggedType::Kind::ZERO_ONE: {
      return arg;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::TAG_PTR: {
      return TaggedType::Even();
    }
    case TaggedType::Kind::VAL:
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::ADDR: {
      return TaggedType::Int();
    }
    case TaggedType::Kind::UNDEF: {
      return arg;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
TaggedType Step::Trunc(Type ty, TaggedType arg)
{
  // Determine whether the type can fit a pointer.
  bool fitsPointer;
  if (target_ && GetSize(ty) < GetSize(target_->GetPointerType())) {
    fitsPointer = false;
  } else {
    fitsPointer = true;
  }
  switch (arg.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::INT:
    case TaggedType::Kind::ZERO_ONE: {
      return arg;
    }
    case TaggedType::Kind::CONST: {
      // TODO: convert to a constant.
      return TaggedType::Int();
    }
    case TaggedType::Kind::MASK: {
      // TODO: truncate mask
      return TaggedType::Int();
    }
    case TaggedType::Kind::VAL: {
      return TaggedType::Int();
    }
    case TaggedType::Kind::UNDEF: {
      return TaggedType::Undef();
    }
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::TAG_PTR:
    case TaggedType::Kind::ADDR: {
      if (fitsPointer) {
        return arg;
      } else {
        return TaggedType::Int();
      }
    }
    case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
  }
  llvm_unreachable("invalid value kind");
}
