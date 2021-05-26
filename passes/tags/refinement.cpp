// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

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
  df_.analyze(dt_);
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
void Refinement::Refine(
    Inst &inst,
    Block *parent,
    Ref<Inst> ref,
    const TaggedType &type)
{
  if (pdt_.dominates(parent, ref->getParent())) {
    // If the definition is post-dominated by the use, change its type.
    analysis_.Refine(ref, type);
    auto *source = &*ref;
    if (inQueue_.insert(source).second) {
      queue_.push(source);
    }
  } else {
    #ifndef NDEBUG
    bool changed = false;
    #endif

    // Find the post-dominated nodes which are successors of the frontier.
    llvm::SmallPtrSet<Block *, 8> blocks;
    for (auto *front : pdf_.calculate(pdt_, pdt_.getNode(parent))) {
      for (auto *succ : front->successors()) {
        if (pdt_.dominates(parent, succ)) {
          blocks.insert(succ);
        }
      }
    }

    // Find the set of nodes which lead into a use of the references.
    llvm::SmallPtrSet<Block *, 8> live;
    {
      std::queue<Block *> q;
      for (Use &use : ref->uses()) {
        if ((*use).Index() != ref.Index()) {
          continue;
        }
        if (auto *phi = ::cast_or_null<PhiInst>(use.getUser())) {
          for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
            if (phi->GetValue(i) == ref) {
              q.push(phi->GetBlock(i));
            }
          }
        } else {
          q.push(::cast<Inst>(use.getUser())->getParent());
        }
      }
      while (!q.empty()) {
        Block *b = q.front();
        q.pop();
        if (blocks.count(b) || b == ref->getParent()) {
          continue;
        }
        if (live.insert(b).second) {
          for (Block *pred : b->predecessors()) {
            q.push(pred);
          }
        }
      }
    }

    // Place the PHIs for the blocks.
    std::unordered_map<Block *, PhiInst *> phis;
    llvm::SmallVector<PhiInst *, 4> newPhis;
    {
      std::queue<Block *> q;
      for (Block *block : blocks) {
        q.push(block);
      }
      while (!q.empty()) {
        Block *block = q.front();
        q.pop();
        for (auto front : df_.calculate(dt_, dt_.getNode(block))) {
          if (live.count(front)) {
            auto *phi = new PhiInst(ref.GetType(), {});
            for (Block *pred : front->predecessors()) {
              phi->Add(pred, ref);
            }
            phis.emplace(front, phi);
            newPhis.push_back(phi);
            front->AddPhi(phi);
          }
        }
      }
    }

    std::stack<Inst *> defs;
    llvm::SmallVector<MovInst *, 4> newMovs;
    std::function<void(Block *)> rewrite = [&](Block *block)
    {
      Block::iterator begin;
      if (blocks.count(block)) {
        // Register the value, if defined in block.
        auto *mov = new MovInst(ref.GetType(), ref, {});
        block->insert(mov, block->first_non_phi());
        defs.push(mov);
        newMovs.push_back(mov);
        begin = std::next(mov->getIterator());
      } else {
        if (auto it = phis.find(block); it != phis.end()) {
          // Register value, if defined in PHI.
          defs.push(it->second);
        }
        begin = block->first_non_phi();
      }
      // Rewrite, if there are uses to be rewritten.
      if (!defs.empty()) {
        auto mov = defs.top()->GetSubValue(0);
        for (auto it = begin; it != block->end(); ++it) {
          for (Use &use : it->operands()) {
            if (::cast_or_null<Inst>(*use) == ref) {
              #ifndef NDEBUG
              if (use.getUser() == &inst) {
                changed = true;
              }
              #endif
              use = mov;
            }
          }
        }
        for (Block *succ : block->successors()) {
          for (PhiInst &phi : succ->phis()) {
            if (phi.GetValue(block) == ref) {
              phi.Remove(block);
              phi.Add(block, mov);
              #ifndef NDEBUG
              changed = true;
              #endif
            }
          }
        }
      }
      // Rewrite dominated nodes.
      for (auto *child : *dt_[block]) {
        rewrite(child->getBlock());
      }
      // Remove the definition.
      if (blocks.count(block) || phis.count(block)) {
        defs.pop();
      }
    };
    rewrite(dt_.getRoot());

    // Recompute the types of the users of the refined instructions.
    for (MovInst *mov : newMovs) {
      analysis_.Define(mov->GetSubValue(0), type);
    }
    // Schedule the PHIs to be recomputed.
    for (PhiInst *phi : newPhis) {
      analysis_.BackwardQueue(phi->GetSubValue(0));
    }
    // Trigger an update of anything relying on the reference.
    analysis_.Refine(ref, analysis_.Find(ref));
    #ifndef NDEBUG
    assert(changed && "original use not changed");
    #endif
  }
}

// -----------------------------------------------------------------------------
void Refinement::RefineAddr(Inst &inst, Ref<Inst> addr)
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
      Refine(inst, inst.getParent(), addr, TaggedType::Heap());
      return;
    }
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::ANY: {
      // Refine to PTR.
      Refine(inst, inst.getParent(), addr, TaggedType::Ptr());
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

  if (!IsEquality(cc) || IsOrdered(cc)) {
    if (vl.IsVal() && vr.IsOdd()) {
      Refine(i, i.getParent(), i.GetLHS(), TaggedType::Odd());
      return;
    }
    if (vr.IsVal() && vl.IsOdd()) {
      Refine(i, i.getParent(), i.GetRHS(), TaggedType::Odd());
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitAndInst(AndInst &i)
{
}

// -----------------------------------------------------------------------------
void Refinement::VisitOrInst(OrInst &i)
{
}

// -----------------------------------------------------------------------------
void Refinement::VisitXorInst(XorInst &i)
{
}

// -----------------------------------------------------------------------------
void Refinement::VisitMovInst(MovInst &i)
{
  if (auto arg = ::cast_or_null<Inst>(i.GetArg())) {
    auto va = analysis_.Find(arg);
    auto vo = analysis_.Find(i);
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
    if (vphi < vin) {
      Refine(phi, phi.GetBlock(i), ref, vphi);
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitCallSite(CallSite &site)
{
  auto callee = analysis_.Find(site.GetCallee());
  if (callee.IsUnknown()) {
    return;
  }
  RefineAddr(site, site.GetCallee());
}
