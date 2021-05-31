// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/type_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Sub(Type ty, TaggedType vl, TaggedType vr)
{
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::EVEN: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return TaggedType::Unknown();
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ZERO: return TaggedType::Even();
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: return TaggedType::Odd();
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::INT: return TaggedType::Int();
        case TaggedType::Kind::VAL: return TaggedType::Odd();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: return TaggedType::Int();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: return TaggedType::Undef();
        case TaggedType::Kind::PTR_INT: return TaggedType::Int();
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ODD: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return TaggedType::Unknown();
        }
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::EVEN: return TaggedType::Odd();
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ODD: return TaggedType::Even();
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::INT: return TaggedType::Int();
        case TaggedType::Kind::VAL: return TaggedType::Int();
        case TaggedType::Kind::HEAP: return TaggedType::Int();
        case TaggedType::Kind::PTR: return TaggedType::Int();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: return TaggedType::Int();
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::EVEN:     return TaggedType::Odd();
        case TaggedType::Kind::ONE:      return TaggedType::Zero();
        case TaggedType::Kind::ODD:      return TaggedType::Even();
        case TaggedType::Kind::ZERO:     return TaggedType::One();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::ZeroOne();
        case TaggedType::Kind::INT:      return TaggedType::Int();
        case TaggedType::Kind::VAL:      return TaggedType::Even();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:
        case TaggedType::Kind::ANY: return TaggedType::Int();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::EVEN:     return TaggedType::Even();
        case TaggedType::Kind::ONE:      return TaggedType::Odd();
        case TaggedType::Kind::ODD:      return TaggedType::Odd();
        case TaggedType::Kind::ZERO:     return TaggedType::Zero();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Int();
        case TaggedType::Kind::INT:      return TaggedType::Int();
        case TaggedType::Kind::VAL:      return TaggedType::Odd();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:      return TaggedType::Int();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:
        case TaggedType::Kind::ANY:      return TaggedType::Int();
        case TaggedType::Kind::PTR_NULL: return TaggedType::Int();
      }
      llvm_unreachable("invalid value kind");
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
        case TaggedType::Kind::VAL: return TaggedType::PtrInt();
        case TaggedType::Kind::EVEN: return TaggedType::Val();
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ZERO: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: {
          return (ty == Type::V64) ? TaggedType::Val() : TaggedType::PtrInt();
        }
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return TaggedType::Unknown();
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: return TaggedType::Int();
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ZERO: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:   return TaggedType::Int();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::ANY: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT: return TaggedType::Ptr();
        case TaggedType::Kind::ZERO: return TaggedType::Ptr();
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL:  return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: return TaggedType::PtrInt();
        case TaggedType::Kind::PTR: return TaggedType::Int();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: return TaggedType::Undef();
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: {
      return vr.IsUnknown() ? vr : TaggedType::Young();
    }
    case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
    case TaggedType::Kind::PTR_INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return TaggedType::Unknown();
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_ONE: return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     return TaggedType::Int();
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::ANY:      return TaggedType::Any();
        case TaggedType::Kind::PTR:
        case TaggedType::Kind::PTR_INT:
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR_NULL:  {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::EVEN:     return TaggedType::PtrInt();
        case TaggedType::Kind::ODD:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ONE:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::INT:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:      return TaggedType::Int();
        case TaggedType::Kind::ZERO:     return TaggedType::PtrNull();
        case TaggedType::Kind::VAL:      llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:     llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::ANY:      return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ANY: {
      return TaggedType::Any();
    }
  }
  llvm_unreachable("invalid value kind");
}
