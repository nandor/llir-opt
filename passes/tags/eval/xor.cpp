// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Xor(Type ty, TaggedType vl, TaggedType vr)
{
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: return vl;
    case TaggedType::Kind::EVEN: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return vr;
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ZERO: return TaggedType::Even();
        case TaggedType::Kind::ODD: return TaggedType::Odd();
        case TaggedType::Kind::ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::INT:     return TaggedType::Int();
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:     return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ODD: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return vr;
        case TaggedType::Kind::ZERO:    return TaggedType::Odd();
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ODD:  return TaggedType::Even();
        case TaggedType::Kind::EVEN: return TaggedType::Odd();
        case TaggedType::Kind::INT:  return TaggedType::Int();
        case TaggedType::Kind::ONE:  return TaggedType::Even();
        case TaggedType::Kind::VAL:  return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return vr;
        case TaggedType::Kind::ZERO:     return TaggedType::One();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::ZeroOne();
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::ODD: return TaggedType::Even();
        case TaggedType::Kind::EVEN: llvm_unreachable("not implemented");
        case TaggedType::Kind::INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::ONE: return TaggedType::Zero();
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: return vr;
    case TaggedType::Kind::ZERO_ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return vr;
        case TaggedType::Kind::ZERO:    return TaggedType::ZeroOne();
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO_ONE: return vl;
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::ODD: return TaggedType::Int();
        case TaggedType::Kind::EVEN: return TaggedType::Int();
        case TaggedType::Kind::INT: return TaggedType::Int();
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return vr;
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ODD:      return TaggedType::Int();
        case TaggedType::Kind::ZERO:     return vl;
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Int();
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:      return Clamp(TaggedType::PtrInt(), ty);
        case TaggedType::Kind::HEAP:     return Clamp(TaggedType::PtrInt(), ty);
        case TaggedType::Kind::PTR:      return TaggedType::PtrInt();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return vr;
        case TaggedType::Kind::EVEN: llvm_unreachable("not implemented");
        case TaggedType::Kind::INT:     return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ODD: return TaggedType::Val();
        case TaggedType::Kind::ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL: return Clamp(TaggedType::PtrInt(), ty);
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return vr;
        case TaggedType::Kind::EVEN: llvm_unreachable("not implemented");
        case TaggedType::Kind::INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ODD: return TaggedType::PtrInt();
        case TaggedType::Kind::ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO: return TaggedType::Heap();
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: return TaggedType::Int();
        case TaggedType::Kind::PTR: return TaggedType::Int();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return vr;
        case TaggedType::Kind::EVEN: llvm_unreachable("not implemented");
        case TaggedType::Kind::INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ODD: return TaggedType::PtrInt();
        case TaggedType::Kind::ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO: return TaggedType::Ptr();
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: return TaggedType::Int();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
    case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
    case TaggedType::Kind::PTR_INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return vr;
        case TaggedType::Kind::ZERO: return vl;
        case TaggedType::Kind::EVEN: return vl;
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::INT: return vl;
        case TaggedType::Kind::PTR_INT: return vl;
        case TaggedType::Kind::VAL: return vl;
        case TaggedType::Kind::HEAP: return TaggedType::Int();
        case TaggedType::Kind::PTR: return vl;
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR_NULL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: llvm_unreachable("not implemented");
        case TaggedType::Kind::EVEN: llvm_unreachable("not implemented");
        case TaggedType::Kind::ODD: llvm_unreachable("not implemented");
        case TaggedType::Kind::INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
  }
  llvm_unreachable("invalid value kind");
}
