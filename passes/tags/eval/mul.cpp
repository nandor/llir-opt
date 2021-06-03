// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Mul(TaggedType vl, TaggedType vr)
{
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return TaggedType::Unknown();
  }
  if (vl.IsZero() || vr.IsZero()) {
    return TaggedType::Zero();
  }
  if (vl.IsOne() || vr.IsOne()) {
    auto other = vl.IsOne() ? vr : vl;
    switch (other.GetKind()) {
      case TaggedType::Kind::UNKNOWN:
      case TaggedType::Kind::MOD:
      case TaggedType::Kind::CONST:
      case TaggedType::Kind::INT:
      case TaggedType::Kind::ZERO:
      case TaggedType::Kind::ONE:
      case TaggedType::Kind::ZERO_ONE: {
        return other;
      }
      case TaggedType::Kind::YOUNG:
      case TaggedType::Kind::HEAP:
      case TaggedType::Kind::VAL:
      case TaggedType::Kind::PTR:
      case TaggedType::Kind::PTR_INT:
      case TaggedType::Kind::PTR_NULL:
      case TaggedType::Kind::ADDR:
      case TaggedType::Kind::TAG_PTR: {
        return TaggedType::Int();
      }
      case TaggedType::Kind::UNDEF: {
        return TaggedType::Undef();
      }
    }
    llvm_unreachable("invalid kind");
  }
  if (vl.IsMod() && vr.IsMod()) {
    auto ml = vl.GetMod();
    auto mr = vr.GetMod();
    auto g = GCD(ml.Div, mr.Div);
    return TaggedType::Modulo({g, (ml.Rem % g) * (mr.Rem % g) % g});
  }
  if (vl.IsEven() || vr.IsEven()) {
    return TaggedType::Even();
  }
  if (vl.IsOdd() && vr.IsOdd()) {
    return TaggedType::Odd();
  }
  return TaggedType::Int();
}
