// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

using namespace tags;


// -----------------------------------------------------------------------------
TaggedType Step::Add(TaggedType vl, TaggedType vr)
{
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::ZERO_ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::INT:       return TaggedType::Int();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::Int();
        case TaggedType::Kind::VAL:       llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     return TaggedType::Int();
        case TaggedType::Kind::MASK:      return TaggedType::Int();
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO_ONE: return vl;
        case TaggedType::Kind::VAL: return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:      return vr;
        case TaggedType::Kind::PTR_INT:  return vr;
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
        case TaggedType::Kind::CONST:    return TaggedType::Int();
        case TaggedType::Kind::MASK:     return TaggedType::Int();
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:       return TaggedType::PtrInt();
        case TaggedType::Kind::INT:       return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     return TaggedType::PtrInt();
        case TaggedType::Kind::MASK:       return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::INT:      return TaggedType::Ptr();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Ptr();
        case TaggedType::Kind::VAL:      return TaggedType::Ptr();
        case TaggedType::Kind::HEAP:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::Ptr();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:    return TaggedType::Ptr();
        case TaggedType::Kind::MASK:     return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::Ptr();
        case TaggedType::Kind::VAL:       llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:   return TaggedType::Ptr();
        case TaggedType::Kind::PTR_NULL:  return TaggedType::PtrInt();
        case TaggedType::Kind::CONST:     return TaggedType::Ptr();
        case TaggedType::Kind::MASK:      return TaggedType::Ptr();
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
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
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::PTR:      return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
        case TaggedType::Kind::CONST:    return TaggedType::PtrInt();
        case TaggedType::Kind::MASK:     return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR_NULL:  {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::PtrInt();
        case TaggedType::Kind::INT:      return TaggedType::PtrInt();
        case TaggedType::Kind::PTR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL:      llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:     llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:    llvm_unreachable("not implemented");
        case TaggedType::Kind::MASK:      return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::MASK: {
      const auto ml = vl.GetMask();
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Int();
        case TaggedType::Kind::INT:      return TaggedType::Int();
        case TaggedType::Kind::PTR:      return TaggedType::Ptr();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:    return TaggedType::Mask(ml + MaskedType(vr.GetConst()));
        case TaggedType::Kind::MASK:     return TaggedType::Mask(ml + vr.GetMask());
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::CONST: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Int();
        case TaggedType::Kind::INT:      return TaggedType::Int();
        case TaggedType::Kind::PTR:      return TaggedType::Ptr();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
        case TaggedType::Kind::MASK: {
          return TaggedType::Mask(MaskedType(vl.GetConst()) + vr.GetMask());
        }
        case TaggedType::Kind::CONST: {
          return TaggedType::Const(vl.GetConst() + vr.GetConst());
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
    case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
  }
  llvm_unreachable("invalid value kind");
}
