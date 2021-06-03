// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::And(Type ty, TaggedType vl, TaggedType vr)
{
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return TaggedType::Unknown();
    }
    case TaggedType::Kind::MOD: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::INT: return TaggedType::Even();
        case TaggedType::Kind::PTR_INT: return TaggedType::PtrInt();
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_ONE: return TaggedType::Zero();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST: llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD: {
          auto ml = vl.GetMod();
          auto mr = vr.GetMod();
          if (ml.Div % 2 == 0 && mr.Div % 2 == 0) {
            auto mask = std::min(ml.Div, mr.Div);
            return TaggedType::Modulo({
                mask,
                ml.Rem & mr.Rem & (mask - 1)
            });
          } else {
            return TaggedType::Int();
          }
        }
        case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ZERO: return TaggedType::Zero();
        case TaggedType::Kind::ONE: return TaggedType::One();
        case TaggedType::Kind::INT:
        case TaggedType::Kind::PTR_INT:
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::VAL:
        case TaggedType::Kind::HEAP:
        case TaggedType::Kind::PTR:
        case TaggedType::Kind::YOUNG:
        case TaggedType::Kind::UNDEF:
        case TaggedType::Kind::PTR_NULL: return TaggedType::ZeroOne();
        case TaggedType::Kind::CONST: llvm_unreachable("not implemented");
        case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD: {
          if (vr.IsOdd()) {
            return TaggedType::One();
          }
          if (vr.IsEven()) {
            return TaggedType::Zero();
          }
          return TaggedType::ZeroOne();
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: {
      return TaggedType::Zero();
    }
    case TaggedType::Kind::ZERO_ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::ZERO: return TaggedType::Zero();
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::PTR_INT:
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::VAL:
        case TaggedType::Kind::HEAP:
        case TaggedType::Kind::PTR:
        case TaggedType::Kind::YOUNG:
        case TaggedType::Kind::UNDEF:
        case TaggedType::Kind::PTR_NULL: return TaggedType::ZeroOne();
        case TaggedType::Kind::CONST: llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:      return TaggedType::ZeroOne();
        case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:       return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::ZERO:      return TaggedType::Zero();
        case TaggedType::Kind::ZERO_ONE:  return TaggedType::ZeroOne();
        case TaggedType::Kind::ONE:       return TaggedType::ZeroOne();
        case TaggedType::Kind::VAL:       return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     return TaggedType::Int();
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:     llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:       return TaggedType::Int();
        case TaggedType::Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::ZERO:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE:  llvm_unreachable("not implemented");
        case TaggedType::Kind::INT:       return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::ONE:       return TaggedType::ZeroOne();
        case TaggedType::Kind::VAL:       return TaggedType::Val();
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
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::ZERO:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE:  llvm_unreachable("not implemented");
        case TaggedType::Kind::INT:       llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ONE:       return TaggedType::ZeroOne();
        case TaggedType::Kind::VAL:       llvm_unreachable("not implemented");
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
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
        case TaggedType::Kind::INT: return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO: llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE: llvm_unreachable("not implemented");
        case TaggedType::Kind::ONE: return TaggedType::ZeroOne();
        case TaggedType::Kind::VAL: llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST: llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD: return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
    case TaggedType::Kind::UNDEF: llvm_unreachable("not implemented");
    case TaggedType::Kind::PTR_INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return TaggedType::Unknown();
        case TaggedType::Kind::INT:
        case TaggedType::Kind::PTR_INT:  return vl;
        case TaggedType::Kind::ZERO:     return TaggedType::Zero();
        case TaggedType::Kind::ZERO_ONE:
        case TaggedType::Kind::ONE:      return TaggedType::ZeroOne();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     return TaggedType::PtrInt();
        case TaggedType::Kind::PTR:      return TaggedType::PtrInt();
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::CONST:    llvm_unreachable("not implemented");
        case TaggedType::Kind::MOD:      return TaggedType::PtrInt();
        case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR_NULL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   llvm_unreachable("not implemented");
        case TaggedType::Kind::INT:       llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:   llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ZERO_ONE:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ONE:       llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL:       llvm_unreachable("not implemented");
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
    case TaggedType::Kind::CONST: llvm_unreachable("not implemented");
    case TaggedType::Kind::TAG_PTR: llvm_unreachable("not implemented");
    case TaggedType::Kind::ADDR: llvm_unreachable("not implemented");
  }
  llvm_unreachable("invalid value kind");
}
