// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Add(TaggedType vl, TaggedType vr)
{
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::EVEN: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::EVEN:     return TaggedType::Even();
        case TaggedType::Kind::ODD:      return TaggedType::Odd();
        case TaggedType::Kind::INT:      return TaggedType::Int();
        case TaggedType::Kind::ONE:      return TaggedType::Odd();
        case TaggedType::Kind::ZERO:     return TaggedType::Even();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Int();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     return TaggedType::Ptr();
        case TaggedType::Kind::PTR:      return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:    return TaggedType::Heap();
        case TaggedType::Kind::UNDEF:    return vl;
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::ANY:      return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ODD: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::ZERO:     return TaggedType::Odd();
        case TaggedType::Kind::EVEN:     return TaggedType::Odd();
        case TaggedType::Kind::ONE:      return TaggedType::Even();
        case TaggedType::Kind::ODD:      return TaggedType::Even();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Int();
        case TaggedType::Kind::INT:      return TaggedType::Int();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::ANY:      return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::EVEN: return TaggedType::Odd();
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::INT: return TaggedType::Int();
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: return TaggedType::Even();
        case TaggedType::Kind::ZERO: return TaggedType::One();
        case TaggedType::Kind::VAL: return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: return vr;
    case TaggedType::Kind::ZERO_ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Int();
        case TaggedType::Kind::ZERO: return TaggedType::ZeroOne();
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_ONE: return vl;
        case TaggedType::Kind::VAL: return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:
        case TaggedType::Kind::PTR_INT: return vr;
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ZERO: return TaggedType::Val();
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::VAL:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT: return TaggedType::Ptr();
        case TaggedType::Kind::ZERO: return TaggedType::Heap();
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL: return TaggedType::Ptr();
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
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Ptr();
        case TaggedType::Kind::ZERO: return TaggedType::Ptr();
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: return TaggedType::Ptr();
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: {
      return TaggedType::Heap();
    }
    case TaggedType::Kind::UNDEF: {
      return vr.IsUnknown() ? vr : TaggedType::Undef();
    }
    case TaggedType::Kind::PTR_INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::PTR: return TaggedType::PtrInt();
        case TaggedType::Kind::ZERO: return vl;
        case TaggedType::Kind::VAL: return vl;
        case TaggedType::Kind::HEAP: return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ANY: return TaggedType::Any();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR_NULL:  {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::EVEN:     return TaggedType::PtrInt();
        case TaggedType::Kind::ODD:      return TaggedType::PtrInt();
        case TaggedType::Kind::ONE:      return TaggedType::PtrInt();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::PtrInt();
        case TaggedType::Kind::INT:      return TaggedType::PtrInt();
        case TaggedType::Kind::PTR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO:     return TaggedType::PtrNull();
        case TaggedType::Kind::VAL:      llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:     llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::ANY:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ANY: {
      return vl;
    }
  }
  llvm_unreachable("invalid value kind");
}
