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
    // Find the post-dominated nodes which are successors of the frontier.
    std::unordered_map<const Block *, TaggedType> splits;
    for (auto *front : pdf_.calculate(pdt_, pdt_.getNode(parent))) {
      for (auto *succ : front->successors()) {
        if (pdt_.dominates(parent, succ)) {
          splits.emplace(succ, type);
        }
      }
    }

    // Find the set of nodes which lead into a use of the references.
    DefineSplits(ref, splits);
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
void Refinement::RefineInt(Inst &inst, Ref<Inst> addr)
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
      // Refine to ODD.
      Refine(inst, inst.getParent(), addr, TaggedType::Odd());
      return;
    }
    case TaggedType::Kind::PTR_NULL: {
      // Refine to ZERO.
      Refine(inst, inst.getParent(), addr, TaggedType::Zero());
      return;

    }
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::ANY: {
      // Refine to INT.
      Refine(inst, inst.getParent(), addr, TaggedType::Int());
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
void Refinement::VisitSelectInst(SelectInst &i)
{
  auto vo = analysis_.Find(i);
  auto vt = analysis_.Find(i.GetTrue());
  auto vf = analysis_.Find(i.GetFalse());
  if (!(vt <= vo)) {
    Refine(i, i.getParent(), i.GetTrue(), vo);
  }
  if (!(vf <= vo)) {
    Refine(i, i.getParent(), i.GetFalse(), vo);
  }
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
    if (vl.IsPtrLike() && vr.IsPtrUnion()) {
      RefineInt(i, i.GetRHS());
    }
    if (vr.IsPtrLike() && vl.IsPtrUnion()) {
      RefineInt(i, i.GetLHS());
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

// -----------------------------------------------------------------------------
void Refinement::VisitJumpCondInst(JumpCondInst &jcc)
{
  auto *bt = jcc.GetTrueTarget();
  auto *bf = jcc.GetFalseTarget();
  if (auto inst = ::cast_or_null<CmpInst>(jcc.GetCond())) {
    auto l = inst->GetLHS();
    auto r = inst->GetRHS();
    switch (inst->GetCC()) {
      case Cond::EQ: case Cond::UEQ: case Cond::OEQ: {
        return RefineEquality(l, r, bt, bf);
      }
      case Cond::NE: case Cond::UNE: case Cond::ONE: {
        return RefineEquality(l, r, bt, bf);
      }
      default: {
        return;
      }
    }
    llvm_unreachable("invalid condition code");
  }
  if (auto inst = ::cast_or_null<AndInst>(jcc.GetCond())) {
    if (analysis_.Find(jcc.GetCond()).IsZeroOne()) {
      auto l = inst->GetLHS();
      auto r = inst->GetRHS();
      if (analysis_.Find(l).IsOne()) {
        return RefineAndOne(r, jcc.getParent(), bt, bf);
      }
      if (analysis_.Find(r).IsOne()) {
        return RefineAndOne(l, jcc.getParent(), bt, bf);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::RefineEquality(Ref<Inst> lhs, Ref<Inst> rhs, Block *bt, Block *bf)
{
  // TODO
}

// -----------------------------------------------------------------------------
void Refinement::RefineAndOne(Ref<Inst> arg, Block *b, Block *bt, Block *bf)
{
  switch (analysis_.Find(arg).GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::UNDEF:
    case TaggedType::Kind::ANY: {
      return;
    }
    case TaggedType::Kind::ZERO:
    case TaggedType::Kind::ONE:
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ODD: {
      // Can simplify condition here, always 0 or 1.
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP: {
      // Can simplify condition here, always 0.
      return;
    }
    case TaggedType::Kind::ZERO_ONE: {
      Specialise(arg, b, { { TaggedType::One(), bt }, { TaggedType::Zero(), bf } });
      return;
    }
    case TaggedType::Kind::INT: {
      Specialise(arg, b, { { TaggedType::Odd(), bt }, { TaggedType::Even(), bf } });
      return;
    }
    case TaggedType::Kind::VAL: {
      Specialise(arg, b, { { TaggedType::Odd(), bt }, { TaggedType::Heap(), bf } });
      return;
    }
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::PTR_NULL: {
      Specialise(arg, b, { { TaggedType::Ptr(), bt }, { TaggedType::Heap(), bf } });
      return;
    }
    case TaggedType::Kind::PTR_INT: {
      return;
    }
  }
  llvm_unreachable("invalid type kind");
}

#include <llvm/Support/GraphWriter.h>

// -----------------------------------------------------------------------------
void Refinement::Specialise(
    Ref<Inst> ref,
    const Block *from,
    const std::vector<std::pair<TaggedType, Block *>> &branches)
{
  std::unordered_map<const Block *, TaggedType> splits;

  // Filter out split points which can be rewritten to constants.
  {
    auto rewrite = [&, this] (Block *block, std::function<Value *()> &&f) {
      for (Use &use : ref->uses()) {
        if (*use == ref) {
          if (auto phi = ::cast_or_null<PhiInst>(use.getUser())) {
            for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
              if (phi->GetValue(i) == ref) {
                if (dt_.Dominates(from, block, phi->GetBlock(i))) {
                  llvm_unreachable("not implemented");
                  break;
                }
              }
            }
          } else {
            auto parent = ::cast<Inst>(use.getUser())->getParent();
            if (dt_.Dominates(from, block, parent)) {
              llvm_unreachable("not implemented");
            }
          }
        }
      }
    };
    for (auto &[ty, block] : branches) {
      if (ty.IsOne()) {
        rewrite(block, [] { return new ConstantInt(1); });
      } else if (ty.IsZero()) {
        rewrite(block, [] { return new ConstantInt(0); });
      } else {
        if (dt_.Dominates(from, block, block)) {
          splits.emplace(block, ty);
        }
      }
    }
  }

  // Find the set of nodes which lead into a use of the references.
  DefineSplits(ref, splits);
}

// -----------------------------------------------------------------------------
std::set<Block *> Refinement::Liveness(
    Ref<Inst> ref,
    const llvm::SmallPtrSetImpl<const Block *> &defs)
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
  std::set<Block *> live;
  while (!q.empty()) {
    Block *b = q.front();
    q.pop();
    if (defs.count(b) || b == ref->getParent()) {
      continue;
    }
    if (live.insert(b).second) {
      for (Block *pred : b->predecessors()) {
        q.push(pred);
      }
    }
  }
  return live;
}

// -----------------------------------------------------------------------------
void Refinement::DefineSplits(
    Ref<Inst> ref,
    const std::unordered_map<const Block *, TaggedType> &splits)
{
  llvm::SmallPtrSet<const Block *, 8> blocks;
  for (auto &[block, ty] : splits) {
    blocks.insert(block);
  }
  auto live = Liveness(ref, blocks);

  // Place the PHIs for the blocks.
  std::unordered_map<Block *, PhiInst *> phis;
  llvm::SmallVector<PhiInst *, 4> newPhis;
  {
    std::queue<const Block *> q;
    for (const Block *block : blocks) {
      q.push(block);
    }
    while (!q.empty()) {
      const Block *block = q.front();
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
  llvm::SmallVector<std::pair<MovInst *, TaggedType>, 4> newMovs;
  std::function<void(Block *)> rewrite = [&](Block *block)
  {
    Block::iterator begin;
    bool defined = false;
    if (auto it = splits.find(block); it != splits.end()) {
      bool liveOut = false;
      for (Block *b : block->successors()) {
        if (live.count(b)) {
          liveOut = true;
          break;
        }
      }
      // Register the value, if defined in block.
      if (liveOut) {
        auto *mov = new MovInst(ref.GetType(), ref, {});
        block->insert(mov, block->first_non_phi());
        defs.push(mov);
        newMovs.emplace_back(mov, it->second);
        begin = std::next(mov->getIterator());
        defined = true;
      } else {
        begin = block->first_non_phi();
      }
    } else {
      if (auto it = phis.find(block); it != phis.end()) {
        // Register value, if defined in PHI.
        defs.push(it->second);
        defined = true;
      }
      begin = block->first_non_phi();
    }
    // Rewrite, if there are uses to be rewritten.
    if (!defs.empty()) {
      auto mov = defs.top()->GetSubValue(0);
      for (auto it = begin; it != block->end(); ++it) {
        for (Use &use : it->operands()) {
          if (::cast_or_null<Inst>(*use) == ref) {
            use = mov;
          }
        }
      }
      for (Block *succ : block->successors()) {
        for (PhiInst &phi : succ->phis()) {
          if (phi.GetValue(block) == ref) {
            phi.Remove(block);
            phi.Add(block, mov);
          }
        }
      }
    }
    // Rewrite dominated nodes.
    for (auto *child : *dt_[block]) {
      rewrite(child->getBlock());
    }
    // Remove the definition.
    if (defined) {
      defs.pop();
    }
  };
  rewrite(dt_.getRoot());

  // Recompute the types of the users of the refined instructions.
  for (auto &[mov, type] : newMovs) {
    analysis_.Define(mov->GetSubValue(0), type);
  }
  // Schedule the PHIs to be recomputed.
  for (PhiInst *phi : newPhis) {
    analysis_.BackwardQueue(phi->GetSubValue(0));
  }
  // Trigger an update of anything relying on the reference.
  analysis_.Refine(ref, analysis_.Find(ref));

}
