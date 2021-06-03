// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Or(TaggedType vl, TaggedType vr)
{
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
    case TaggedType::Kind::MOD: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::ZERO:      return TaggedType::Even();
        case TaggedType::Kind::ONE:       return TaggedType::Odd();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::Int();
        case TaggedType::Kind::INT:       return TaggedType::Int();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:       return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD: {
          if (vl == vr) {
            return vl;
          } else {
            llvm_unreachable("not implemented");
          }
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::ZERO:      return TaggedType::One();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::One();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::INT:       return TaggedType::Odd();
        case TaggedType::Kind::ONE:       return TaggedType::Odd();
        case TaggedType::Kind::VAL:       return TaggedType::Val();
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD: {
          auto mr = vr.GetMod();
          if (mr.Div % 2 == 0) {
            return TaggedType::Modulo({ mr.Div, mr.Rem | 1 });
          } else {
            return TaggedType::Int();
          }
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: {
      return vr;
    }
    case TaggedType::Kind::ZERO_ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::ZERO:      return TaggedType::ZeroOne();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::ZeroOne();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::ONE:       return TaggedType::One();
        case TaggedType::Kind::INT:       return TaggedType::Int();
        case TaggedType::Kind::VAL:       return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:      return TaggedType::PtrInt();
        case TaggedType::Kind::PTR:       return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     return TaggedType::Undef();
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:       return TaggedType::Int();
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:       return TaggedType::Int();
        case TaggedType::Kind::ZERO:      return TaggedType::Int();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::Int();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::ONE:       return TaggedType::Odd();
        case TaggedType::Kind::VAL:       return TaggedType::Val();
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:       return TaggedType::Int();
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:       return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::ONE:       return TaggedType::Val();
        case TaggedType::Kind::ZERO:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE:  llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL:       return TaggedType::Val();
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:       return TaggedType::PtrInt();
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::INT:      return TaggedType::Ptr();
        case TaggedType::Kind::PTR_INT:  return TaggedType::Ptr();
        case TaggedType::Kind::ONE:      return TaggedType::Ptr();
        case TaggedType::Kind::ZERO:     return TaggedType::Heap();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Ptr();
        case TaggedType::Kind::MOD:      return TaggedType::PtrInt();
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
    }
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::MOD:      return TaggedType::PtrInt();
        case TaggedType::Kind::INT:      return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::ONE:      return TaggedType::Ptr();
        case TaggedType::Kind::ZERO:     return TaggedType::Ptr();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Ptr();
        case TaggedType::Kind::VAL:      llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:      return TaggedType::PtrInt();
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:    llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
    case TaggedType::Kind::UNDEF: {
      return vr.IsUnknown() ? TaggedType::Unknown() : TaggedType::Undef();
    }
    case TaggedType::Kind::PTR_INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::ZERO:      return TaggedType::PtrInt();
        case TaggedType::Kind::INT:       return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::ONE:       return TaggedType::PtrInt();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:       return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:      return TaggedType::PtrInt();
        case TaggedType::Kind::PTR:       return TaggedType::PtrInt();
        case TaggedType::Kind::MOD:       return TaggedType::PtrInt();
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR_NULL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   llvm_unreachable("not implemented");
        case TaggedType::Kind::INT:       llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::ZERO:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ONE:       return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:       llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:       llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::CONST:    llvm_unreachable("not implemented");
    case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
    case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
  }
  llvm_unreachable("invalid value kind");
}
