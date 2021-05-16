// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/object.h"
#include "core/data.h"
#include "passes/tags/refinement.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/analysis.h"

using namespace tags;


// -----------------------------------------------------------------------------
Refinement::Refinement(TypeAnalysis &analysis, const Target *target, Func &func)
  : analysis_(analysis)
  , target_(target)
  , func_(func)
  , pdt_(func_)
{
  pdf_.analyze(pdt_);
}

// -----------------------------------------------------------------------------
void Refinement::Run()
{
  for (Block &block : func_) {
    for (Inst &inst : block) {
      Dispatch(inst);
    }
  }

  while (!queue_.empty()) {
    auto *inst = queue_.front();
    queue_.pop();
    inQueue_.erase(inst);
    Dispatch(*inst);
  }
}

// -----------------------------------------------------------------------------
void Refinement::Refine(Inst &i, Ref<Inst> ref, const TaggedType &type)
{
  if (pdt_.dominates(i.getParent(), ref->getParent())) {
    analysis_.Refine(ref, type);
    auto *source = &*ref;
    if (inQueue_.insert(source).second) {
      queue_.push(source);
    }
  } else {
    // TODO
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitLoadInst(LoadInst &i)
{
  switch (analysis_.Find(i.GetAddr()).GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::ZERO:
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ONE:
    case TaggedType::Kind::ODD:
    case TaggedType::Kind::ZERO_ONE:
    case TaggedType::Kind::INT:
    case TaggedType::Kind::UNDEF: {
      // This is a trap, handled elsewhere.
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::PTR: {
      // Already a pointer, nothing to refine.
      return;
    }
    case TaggedType::Kind::VAL: {
      // Refine to HEAP.
      Refine(i, i.GetAddr(), TaggedType::Heap());
      return;
    }
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::ANY: {
      // Refine to PTR.
      Refine(i, i.GetAddr(), TaggedType::Ptr());
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitCmpInst(CmpInst &i)
{
  auto cc = i.GetCC();
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());

  bool isEquality = cc == Cond::EQ || cc == Cond::NE;
  if (!isEquality) {
    if (vl.IsVal() && vr.IsOdd()) {
      Refine(i, i.GetLHS(), TaggedType::Odd());
      return;
    }
    if (vr.IsVal() && vl.IsOdd()) {
      Refine(i, i.GetRHS(), TaggedType::Odd());
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitPhiInst(PhiInst &phi)
{
  auto vphi = analysis_.Find(phi);
  if (vphi.IsUnknown()) {
    return;
  }

  for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
    auto ref = phi.GetValue(i);
    auto vin = analysis_.Find(ref);
    if (vphi < vin) {
      Refine(phi, ref, vphi);
    }
  }
}
