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
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::INT: return TaggedType::Int();
        case TaggedType::Kind::ONE: return TaggedType::Even();
        case TaggedType::Kind::ZERO: return TaggedType::One();
        case TaggedType::Kind::VAL: return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST: llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD: {
          auto mr = vr.GetMod();
          return TaggedType::Modulo({ mr.Div, (mr.Rem + 1) % mr.Div });
        }
        case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: return vr;
    case TaggedType::Kind::ZERO_ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ONE:       return TaggedType::Int();
        case TaggedType::Kind::INT:       return TaggedType::Int();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::Int();
        case TaggedType::Kind::ZERO:      return TaggedType::ZeroOne();
        case TaggedType::Kind::VAL:       llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:       return TaggedType::Int();
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_ONE: return vl;
        case TaggedType::Kind::VAL: return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:      return vr;
        case TaggedType::Kind::PTR_INT:  return vr;
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
        case TaggedType::Kind::CONST:    llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:      return TaggedType::Int();
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::ZERO:      return TaggedType::Val();
        case TaggedType::Kind::ONE:       return TaggedType::PtrInt();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:       return TaggedType::PtrInt();
        case TaggedType::Kind::INT:       return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:       return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:      return TaggedType::Ptr();
        case TaggedType::Kind::ZERO:     return TaggedType::Heap();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Ptr();
        case TaggedType::Kind::VAL:      return TaggedType::Ptr();
        case TaggedType::Kind::HEAP:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::Ptr();
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:    llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:      return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Ptr();
        case TaggedType::Kind::ZERO: return TaggedType::Ptr();
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::Ptr();
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
        case TaggedType::Kind::CONST: llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:      return TaggedType::Ptr();
        case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
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
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::PTR:      return TaggedType::PtrInt();
        case TaggedType::Kind::ZERO:     return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
        case TaggedType::Kind::CONST: llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:      return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR_NULL:  {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
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
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:    llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:      return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::MOD: {
      const auto ml = vl.GetMod();
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Int();
        case TaggedType::Kind::INT:      return TaggedType::Int();
        case TaggedType::Kind::PTR:      return TaggedType::Ptr();
        case TaggedType::Kind::ZERO:     return vl;
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     return TaggedType::Ptr();
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_NULL: return TaggedType::PtrInt();
        case TaggedType::Kind::CONST:    llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:     llvm_unreachable("not implemented");
        case TaggedType::Kind::ONE: {
          return TaggedType::Modulo({ ml.Div, (ml.Rem + 1) % ml.Div });
        }
        case TaggedType::Kind::MOD: {
          auto mr = vr.GetMod();
          auto lcm = LCM(ml.Div, mr.Div);
          return TaggedType::Modulo({
              lcm,
              (ml.Rem * mr.Div + mr.Rem * ml.Div) % lcm
          });
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::CONST: llvm_unreachable("not implemented");
    case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
    case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
  }
  llvm_unreachable("invalid value kind");
}
