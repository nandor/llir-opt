// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/type_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Shr(Type ty, TaggedType vl, TaggedType vr)
{
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ODD: {
      return TaggedType::Int();
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return TaggedType::Unknown();
        }
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::EVEN: {
          return TaggedType::ZeroOne();
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          return TaggedType::Zero();
        }
        case TaggedType::Kind::ZERO: {
          return TaggedType::One();
        }
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::ANY: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: {
      return vr.IsUnknown() ? vr : TaggedType::Zero();
    }
    case TaggedType::Kind::ZERO_ONE:
    case TaggedType::Kind::INT: {
      return vr.IsUnknown() ? vr : TaggedType::Int();
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return TaggedType::Unknown();
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT: {
          // TODO: distinguish even from constant.
          return TaggedType::Int();
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          return TaggedType::Int();
        }
        case TaggedType::Kind::PTR_INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::ANY: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return TaggedType::Unknown();
        }
        case TaggedType::Kind::EVEN: llvm_unreachable("not implemented");
        case TaggedType::Kind::INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::ODD: llvm_unreachable("not implemented");
        case TaggedType::Kind::ONE: {
          return TaggedType::Int();
        }
        case TaggedType::Kind::PTR_INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::ANY: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
    case TaggedType::Kind::UNDEF: return TaggedType::Undef();
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::ANY:
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
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::ONE:
    case TaggedType::Kind::ODD: {
      return TaggedType::Even();
    }
    case TaggedType::Kind::INT:
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ZERO:
    case TaggedType::Kind::ZERO_ONE: {
      return TaggedType::Int();
    }
    case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
    case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
    case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
    case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
    case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
    case TaggedType::Kind::ANY: llvm_unreachable("not implemented");
    case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
  }
  llvm_unreachable("invalid value kind");
}
