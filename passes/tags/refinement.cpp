// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/object.h"
#include "core/data.h"
#include "passes/tags/refinement.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/type_analysis.h"

using namespace tags;


// -----------------------------------------------------------------------------
Refinement::Refinement(TypeAnalysis &analysis, const Target *target, Func &func)
  : analysis_(analysis)
  , target_(target)
  , func_(func)
  , dt_(func_)
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
  auto *parent = i.getParent();
  if (pdt_.dominates(parent, ref->getParent())) {
    // If the definition is post-dominated by the use, change its type.
    analysis_.Refine(ref, type);
    auto *source = &*ref;
    if (inQueue_.insert(source).second) {
      queue_.push(source);
    }
  } else {
    // Split live ranges, inserting moves at post-doms which follow the
    // dominance frontier.
    llvm::SmallPtrSet<Block *, 8> blocks;
    for (auto *front : pdf_.calculate(pdt_, pdt_.getNode(parent))) {
      for (auto *succ : front->successors()) {
        if (pdt_.dominates(parent, succ)) {
          blocks.insert(succ);
        }
      }
    }
    for (Block *block : blocks) {
      llvm::SmallVector<Use *, 8> uses;
      for (Use &use : ref->uses()) {
        if (dt_.dominates(block, ::cast<Inst>(use.getUser())->getParent())) {
          uses.push_back(&use);
        }
      }
      if (!uses.empty()) {
        auto *mov = new MovInst(ref.GetType(), ref, {});
        block->insert(mov, block->first_non_phi());
        for (Use *use : uses) {
          *use = mov->GetSubValue(0);
        }
        analysis_.Refine(mov->GetSubValue(0), type);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::RefineAddr(Inst &i, Ref<Inst> addr)
{
  switch (analysis_.Find(addr).GetKind()) {
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
      Refine(i, addr, TaggedType::Heap());
      return;
    }
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::ANY: {
      // Refine to PTR.
      Refine(i, addr, TaggedType::Ptr());
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitMemoryLoadInst(MemoryLoadInst &i)
{
  RefineAddr(i, i.GetAddr());
}

// -----------------------------------------------------------------------------
void Refinement::VisitMemoryStoreInst(MemoryStoreInst &i)
{
  RefineAddr(i, i.GetAddr());
}

// -----------------------------------------------------------------------------
void Refinement::VisitSubInst(SubInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
  if (vo.IsPtr() && vl.IsPtrUnion() && vr.IsIntLike()) {
    RefineAddr(i, i.GetLHS());
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitAddInst(AddInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
  if (vo.IsPtr()) {
    if (vl.IsIntLike() && vr.IsPtrUnion()) {
      RefineAddr(i, i.GetRHS());
    }
    if (vr.IsIntLike() && vl.IsPtrUnion()) {
      RefineAddr(i, i.GetLHS());
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

  auto *parent = phi.getParent();
  for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
    auto ref = phi.GetValue(i);
    auto vin = analysis_.Find(ref);
    if (vphi < vin && pdt_.Dominates(phi.GetBlock(i), parent, ref->getParent())) {
      Refine(phi, ref, vphi);
    }
  }
}
