// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/type_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Ext(Type ty, TaggedType arg)
{
  switch (arg.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ODD:
    case TaggedType::Kind::INT:
    case TaggedType::Kind::ZERO:
    case TaggedType::Kind::ONE:
    case TaggedType::Kind::ZERO_ONE: {
      return arg;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP: {
      return TaggedType::Even();
    }
    case TaggedType::Kind::VAL:
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::PTR_NULL: {
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
    case TaggedType::Kind::ZERO:
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ODD:
    case TaggedType::Kind::ONE:
    case TaggedType::Kind::ZERO_ONE:
    case TaggedType::Kind::INT: {
      return arg;
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
    case TaggedType::Kind::PTR_NULL: {
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
