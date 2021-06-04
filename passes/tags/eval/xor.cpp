// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Xor(TaggedType vl, TaggedType vr)
{
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: return TaggedType::Unknown();
    case TaggedType::Kind::UNDEF:   return TaggedType::Undef();
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:  return vr;
        case TaggedType::Kind::PTR_INT:  return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:      return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:     return TaggedType::PtrInt();
        case TaggedType::Kind::PTR:      return TaggedType::PtrInt();
        case TaggedType::Kind::YOUNG:    llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:    llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_INT:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::INT:      return TaggedType::Mask(vl.GetInt() ^ vr.GetInt());
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:       return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:       return TaggedType::Val();
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_INT:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:       llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:   llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL:       llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_INT:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_NULL: llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:       return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:   return TaggedType::PtrInt();
        case TaggedType::Kind::VAL:       llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       return TaggedType::Int();
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  return TaggedType::PtrInt();
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_INT:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: llvm_unreachable("not implemented");
    case TaggedType::Kind::PTR_INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:       return TaggedType::PtrInt();
        case TaggedType::Kind::PTR_INT:   return vl;
        case TaggedType::Kind::VAL:       return vl;
        case TaggedType::Kind::HEAP:      return TaggedType::Int();
        case TaggedType::Kind::PTR:       return vl;
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  return TaggedType::PtrInt();
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_INT:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR_NULL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:       llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:   llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL:       llvm_unreachable("not implemented");
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_INT:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
    case TaggedType::Kind::ADDR_INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN:   return TaggedType::Unknown();
        case TaggedType::Kind::INT:       llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_INT:   llvm_unreachable("not implemented");
        case TaggedType::Kind::VAL:       return TaggedType::PtrInt();
        case TaggedType::Kind::HEAP:      llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR:       llvm_unreachable("not implemented");
        case TaggedType::Kind::YOUNG:     llvm_unreachable("not implemented");
        case TaggedType::Kind::UNDEF:     llvm_unreachable("not implemented");
        case TaggedType::Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR:      llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_INT:  llvm_unreachable("not implemented");
        case TaggedType::Kind::ADDR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ADDR_NULL: llvm_unreachable("not implemented");
  }
  llvm_unreachable("invalid value kind");
}
