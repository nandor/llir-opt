// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>
#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/PostOrderIterator.h>

#include "core/atom.h"
#include "core/object.h"
#include "core/data.h"
#include "passes/tags/refinement.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/register_analysis.h"

using namespace tags;

#define DEBUG_TYPE "eliminate-tags"

STATISTIC(NumMovsRefined, "Number of movs refined");

//.L180$local94132


// -----------------------------------------------------------------------------
static Type ToType(Type ty, TaggedType kind)
{
  if (ty == Type::V64 && kind.IsOddLike()) {
    return Type::I64;
  } else {
    return ty;
  }
}

// -----------------------------------------------------------------------------
static std::optional<TaggedType> RefineMovTo(
    const TaggedType &vmov,
    const TaggedType &varg,
    Type type)
{
  if (vmov < varg) {
    return vmov;
  }
  if (varg.IsAddrInt() && type == Type::V64) {
    return TaggedType::Odd();
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
static std::optional<std::pair<TaggedType, bool>>
RefineJoinTo(const TaggedType &orig, const TaggedType &ty, Type type)
{
  switch (orig.GetKind()) {
    case TaggedType::Kind::INT: {
      auto io = orig.GetInt();
      auto iok = io.GetKnown(), iov = io.GetValue();
      switch (ty.GetKind()) {
        case TaggedType::Kind::UNKNOWN:
        case TaggedType::Kind::UNDEF: {
          return std::nullopt;
        }
        case TaggedType::Kind::INT: {
          auto it = ty.GetInt();
          auto itk = it.GetKnown(), itv = it.GetValue();
          // The known bits of ty must be the same as the known bits of orig.
          assert(((iok & itk & iov) == (iok & itk & itv)) && "invalid integer join");
          uint64_t newKnown = itk & ~iok;
          uint64_t newVal = iov;
          // If refining to a value, set the last bit as well.
          if (type == Type::V64) {
            newKnown |= (iok & 1) != 1;
            newVal |= 1;
          }
          if (newKnown) {
            return std::make_pair(
                TaggedType::Mask({ (itk & newKnown & itv) | newVal, newKnown | iok }),
                false
            );
          }
          return std::nullopt;
        }
        case TaggedType::Kind::VAL: {
          if (!(iok & 1)) {
            return std::make_pair(
                TaggedType::Mask({ iov | 1, iok | 1 }),
                false
            );
          } else {
            assert((iov & 1) && "invalid integer to val");
            return std::nullopt;
          }
        }
        case TaggedType::Kind::FUNC:
        case TaggedType::Kind::YOUNG:
        case TaggedType::Kind::HEAP:
        case TaggedType::Kind::ADDR:
        case TaggedType::Kind::PTR: {
          return std::make_pair(ty, true);
        }
        case TaggedType::Kind::HEAP_OFF:
        case TaggedType::Kind::ADDR_NULL:
        case TaggedType::Kind::ADDR_INT:
        case TaggedType::Kind::PTR_NULL:
        case TaggedType::Kind::PTR_INT: {
          return std::nullopt;
        }
      }
      llvm_unreachable("invalid type kind");
    }
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::UNDEF: {
      return std::nullopt;
    }
    case TaggedType::Kind::FUNC:
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::PTR: {
      if (ty.IsInt()) {
        return std::make_pair(ty, true);
      }
      if (ty < orig) {
        return std::make_pair(ty, false);
      }
      return std::nullopt;
    }
    case TaggedType::Kind::ADDR_NULL: {
      if (ty.IsPtrLike()) {
        return std::make_pair(TaggedType::Addr(), false);
      }
      if (ty.IsInt()) {
        return std::make_pair(ty, true);
      }
      if (ty.IsFunc()) {
        return std::make_pair(ty, false);
      }
      return std::nullopt;
    }
    case TaggedType::Kind::ADDR_INT: {
      if (ty.IsVal() || type == Type::V64) {
        return std::make_pair(TaggedType::Odd(), false);
      }
      if (ty.IsPtrLike()) {
        return std::make_pair(TaggedType::Addr(), false);
      }
      if (ty.IsInt() || ty.IsFunc()) {
        return std::make_pair(ty, false);
      }
      return std::nullopt;
    }
    case TaggedType::Kind::VAL: {
      if (ty.IsPtrLike()) {
        return std::make_pair(TaggedType::Heap(), false);
      }
      if (ty.IsInt()) {
        return std::make_pair(TaggedType::Odd(), false);
      }
      return std::nullopt;
    }
    case TaggedType::Kind::PTR_NULL: {
      if (ty.IsPtrLike()) {
        return std::make_pair(TaggedType::Ptr(), false);
      }
      if (ty.IsInt() || ty.IsFunc()) {
        return std::make_pair(ty, false);
      }
      return std::nullopt;
    }
    case TaggedType::Kind::PTR_INT: {
      if (ty.IsVal() || type == Type::V64) {
        return std::make_pair(TaggedType::Val(), false);
      }
      if (ty.IsPtrLike()) {
        return std::make_pair(TaggedType::Ptr(), false);
      }
      if (ty.IsInt() || ty.IsFunc()) {
        return std::make_pair(ty, false);
      }
      return std::nullopt;
    }
  }
  llvm_unreachable("invalid type kind");
}

// -----------------------------------------------------------------------------
Refinement::Refinement(RegisterAnalysis &analysis, const Target *target, Func &func)
  : analysis_(analysis)
  , target_(target)
  , func_(func)
{
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
    while (!queue_.empty()) {
      auto *inst = queue_.front();
      queue_.pop();
      inQueue_.erase(inst);
      Dispatch(*inst);
    }
    PullFrontier();
  }
}

// -----------------------------------------------------------------------------
void Refinement::Refine(
    Block *parent,
    Ref<Inst> ref,
    const TaggedType &nt)
{
  auto *func = parent->getParent();
  assert(func == ref->getParent()->getParent() && "invalid block");
  assert(analysis_.Find(ref) != nt && "no refinement");

  auto &doms = analysis_.GetDoms(*func);
  if (doms.PDT.dominates(parent, ref->getParent())) {
    Refine(ref, nt);
  } else {
    // Find the post-dominated nodes which are successors of the frontier.
    std::unordered_map<const Block *, TaggedType> splits;
    for (auto *front : doms.PDF.calculate(doms.PDT, doms.PDT.getNode(parent))) {
      for (auto *succ : front->successors()) {
        if (doms.PDT.dominates(parent, succ)) {
          splits.emplace(succ, nt);
        }
      }
    }

    // Find the set of nodes which lead into a use of the references.
    DefineSplits(doms, ref, splits);
  }
}

// -----------------------------------------------------------------------------
void Refinement::Refine(
    Block *start,
    Block *end,
    Ref<Inst> ref,
    const TaggedType &nt)
{
  auto *func = ref->getParent()->getParent();
  assert(start->getParent() == func && "invalid block");
  assert(end->getParent() == func && "invalid block");
  assert(analysis_.Find(ref) != nt && "no refinement");

  // Refine the value.
  auto &doms = analysis_.GetDoms(*func);
  if (doms.PDT.Dominates(start, end, ref->getParent())) {
    Refine(ref, nt);
  } else if (auto *node = doms.PDT.getNode(start)) {
    // Find the post-dominated nodes which are successors of the frontier.
    std::unordered_map<const Block *, TaggedType> splits;
    for (auto *front : doms.PDF.calculate(doms.PDT, node)) {
      for (auto *succ : front->successors()) {
        if (doms.PDT.Dominates(start, end, succ)) {
          splits.emplace(succ, nt);
        }
      }
    }
    if (splits.empty()) {
      // Create a block along the edge.
      auto *split = new Block(start->getName());
      func->insertAfter(start->getIterator(), split);
      split->AddInst(new JumpInst(end, {}));

      // Rewrite the terminator of the start.
      {
        auto *term = start->GetTerminator();
        for (auto it = term->op_begin(); it != term->op_end(); ) {
          Use &use = *it++;
          if ((*use).Get() == end) {
            use = split;
          }
        }
      }
      // Rewrite PHIs of end.
      for (auto &phi : end->phis()) {
        auto v = phi.GetValue(start);
        phi.Remove(start);
        phi.Add(split, v);
      }
      // Add a mov along this edge.
      DefineSplits(analysis_.RebuildDoms(*func), ref, { { split, nt } });
    } else {
      // Introduce the movs.
      DefineSplits(doms, ref, splits);
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::Refine(Ref<Inst> ref, const TaggedType &nt)
{
  auto *block = ref->getParent();

  // If the refinement creates an integer-to-pointer cast, add an explicit
  // mov instruction to perform it, carrying information through type.
  auto ot = analysis_.Find(ref);
  if (nt < ot) {
    // If the definition is post-dominated by the use, change its type.
    analysis_.Refine(ref, nt);
    auto *source = &*ref;
    if (inQueue_.insert(source).second) {
      queue_.push(source);
    }
  } else {
    auto *newMov = new MovInst(ToType(ref.GetType(), nt), ref, ref->GetAnnots());
    block->insert(newMov, std::next(ref->getIterator()));
    for (auto ut = ref->use_begin(); ut != ref->use_end(); ) {
      Use &use = *ut++;
      if (use.getUser() != newMov) {
        use = newMov->GetSubValue(0);
      }
    }
    analysis_.Define(newMov->GetSubValue(0), nt);
  }
}

// -----------------------------------------------------------------------------
void Refinement::RefineAddr(Inst &inst, Ref<Inst> addr)
{
  switch (analysis_.Find(addr).GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::UNDEF: {
      // Should trap, nothing to refine.
      return;
    }
    case TaggedType::Kind::INT: {
      // Integer-to-pointer cast, results in UB.
      // Insert a move performing an explicit integer-to-pointer cast.
      Refine(inst.getParent(), addr, TaggedType::Ptr());
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::FUNC: {
      // Already a pointer, nothing to refine.
      return;
    }
    case TaggedType::Kind::VAL: {
      // Refine to HEAP.
      Refine(inst.getParent(), addr, TaggedType::Heap());
      return;
    }
    case TaggedType::Kind::ADDR_NULL:
    case TaggedType::Kind::ADDR_INT: {
      // Refine to ADDR.
      Refine(inst.getParent(), addr, TaggedType::Addr());
      return;
    }
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT: {
      // Refine to PTR.
      Refine(inst.getParent(), addr, TaggedType::Ptr());
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::RefineInt(Inst &inst, Ref<Inst> addr)
{
  switch (analysis_.Find(addr).GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::UNDEF: {
      // Should trap, nothing to refine.
    }
    case TaggedType::Kind::INT: {
      // Already an integer, nothing to refine.
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::FUNC: {
      // Add an explicit pointer-to-integer cast.
      Refine(inst.getParent(), addr, TaggedType::Int());
      return;
    }
    case TaggedType::Kind::VAL: {
      // Refine to ODD.
      Refine(inst.getParent(), addr, TaggedType::Odd());
      return;
    }
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::ADDR_NULL: {
      // Refine to ZERO.
      Refine(inst.getParent(), addr, TaggedType::Zero());
      return;
    }
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::ADDR_INT: {
      // Refine to INT.
      Refine(inst.getParent(), addr, TaggedType::Int());
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::RefineFunc(Inst &inst, Ref<Inst> addr)
{
  switch (analysis_.Find(addr).GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::UNDEF: {
      // Should trap, nothing to refine.
      return;
    }
    case TaggedType::Kind::FUNC: {
      // Already a pointer, nothing to refine.
      return;
    }
    case TaggedType::Kind::INT:
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::VAL:
    case TaggedType::Kind::ADDR_NULL:
    case TaggedType::Kind::ADDR_INT:
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT: {
      // Refine to PTR.
      Refine(inst.getParent(), addr, TaggedType::Func());
      return;
    }
  }
}
// -----------------------------------------------------------------------------
void Refinement::RefineEquality(
    Ref<Inst> lhs,
    Ref<Inst> rhs,
    Block *b,
    Block *bt,
    Block *bf)
{
  auto vl = analysis_.Find(lhs);
  auto vr = analysis_.Find(rhs);
  if (!vl.IsUnknown() && vl < vr) {
    Specialise(rhs, b, { { vl, bt } });
    return;
  }
  if (!vr.IsUnknown() && vr < vl) {
    Specialise(lhs, b, { { vr, bt } });
    return;
  }
}

// -----------------------------------------------------------------------------
void Refinement::RefineInequality(
    Ref<Inst> lhs,
    Ref<Inst> rhs,
    Block *b,
    Block *blt,
    Block *bgt)
{
  auto vl = analysis_.Find(lhs);
  auto vr = analysis_.Find(rhs);
  if (vl.IsVal() && vr.IsOdd()) {
    return Specialise(lhs, b, { { TaggedType::Odd(), blt } });
  }
  if (vr.IsVal() && vl.IsOdd()) {
    return Specialise(rhs, b, { { TaggedType::Odd(), bgt } });
  }
}

// -----------------------------------------------------------------------------
void Refinement::RefineAndOne(Ref<Inst> arg, Block *b, Block *bt, Block *bf)
{
  auto ty = analysis_.Find(arg);
  switch (ty.GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::UNDEF: {
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::PTR: {
      // Can simplify condition here, always 0.
      return;
    }
    case TaggedType::Kind::INT: {
      auto i = ty.GetInt();
      auto v = i.GetValue();
      auto k = i.GetKnown();
      auto lhs = TaggedType::Mask({ v & ~1, k | 1});
      auto rhs = TaggedType::Mask({ v | 1, k | 1});
      Specialise(arg, b, { { rhs, bt }, { lhs, bf } });
      return;
    }
    case TaggedType::Kind::VAL: {
      Specialise(arg, b, { { TaggedType::Odd(), bt }, { TaggedType::Heap(), bf } });
      return;
    }
    case TaggedType::Kind::FUNC:
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT:
    case TaggedType::Kind::ADDR_NULL:
    case TaggedType::Kind::ADDR_INT: {
      // Cannot refine.
      return;
    }
  }
  llvm_unreachable("invalid type kind");
}

// -----------------------------------------------------------------------------
void Refinement::Specialise(
    Ref<Inst> ref,
    const Block *from,
    const std::vector<std::pair<TaggedType, Block *>> &branches)
{
  assert(from->getParent() == ref->getParent()->getParent() && "invalid block");

  auto &doms = analysis_.GetDoms(*from->getParent());
  std::unordered_map<const Block *, TaggedType> splits;
  for (auto &[ty, block] : branches) {
    if (!doms.DT.Dominates(from, block, block)) {
      continue;
    }
    bool split = false;
    for (auto it = block->first_non_phi(); it != block->end(); ++it) {
      if (auto *mov = ::cast_or_null<MovInst>(&*it)) {
        if (analysis_.Find(mov->GetSubValue(0)) == ty) {
          split = true;
          break;
        }
      }
    }
    if (!split) {
      splits.emplace(block, ty);
    }
  }
  DefineSplits(doms, ref, splits);
}

// -----------------------------------------------------------------------------
std::pair<std::set<Block *>, std::set<Block *>>
Refinement::Liveness(
    Ref<Inst> ref,
    const llvm::SmallPtrSetImpl<const Block *> &defs)
{
  std::queue<Block *> q;
  for (Use &use : ref->uses()) {
    if ((*use).Index() != ref.Index()) {
      continue;
    }
    auto *user = use.getUser();
    if (auto *phi = ::cast_or_null<PhiInst>(user)) {
      for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
        if (phi->GetValue(i) == ref) {
          q.push(phi->GetBlock(i));
        }
      }
    } else {
      q.push(::cast<Inst>(use.getUser())->getParent());
    }
  }
  std::set<Block *> livePhi, liveMov;
  while (!q.empty()) {
    Block *b = q.front();
    q.pop();
    if (b == ref->getParent()) {
      continue;
    }
    if (defs.count(b)) {
      liveMov.insert(b);
      continue;
    }
    if (livePhi.insert(b).second) {
      liveMov.insert(b);
      for (Block *pred : b->predecessors()) {
        q.push(pred);
      }
    }
  }
  return std::make_pair(livePhi, liveMov);
}

// -----------------------------------------------------------------------------
void Refinement::DefineSplits(
    DominatorCache &doms,
    Ref<Inst> ref,
    const std::unordered_map<const Block *, TaggedType> &splits)
{
  llvm::SmallPtrSet<const Block *, 8> blocks;
  for (auto &[block, ty] : splits) {
    blocks.insert(block);
  }

  auto refTy = analysis_.Find(ref);
  const auto &[livePhi, liveMov] = Liveness(ref, blocks);

  // Place the PHIs for the blocks.
  std::unordered_map<Block *, PhiInst *> phis;
  std::unordered_map<PhiInst *, TaggedType> newPhis;
  {
    std::queue<const Block *> q;
    for (const Block *block : blocks) {
      q.push(block);
    }
    while (!q.empty()) {
      const Block *block = q.front();
      q.pop();
      for (auto front : doms.DF.calculate(doms.DT, doms.DT.getNode(block))) {
        if (livePhi.count(front) && !phis.count(front)) {
          auto *phi = new PhiInst(ref.GetType(), {});
          front->AddPhi(phi);
          TaggedType ty = TaggedType::Unknown();
          for (Block *pred : front->predecessors()) {
            phi->Add(pred, ref);
            TaggedType predTy = TaggedType::Unknown();
            for (auto &[block, ty] : splits) {
              if (doms.DT.dominates(block, pred)) {
                predTy |= ty;
              }
            }
            ty |= predTy.IsUnknown() ? refTy : predTy;
          }
          phis.emplace(front, phi);
          newPhis.emplace(phi, ty);
          q.push(front);
        }
      }
    }
  }

  std::stack<Inst *> defs;
  std::unordered_map<MovInst *, TaggedType> newMovs;
  std::function<void(Block *)> rewrite =
    [&, &liveMov = liveMov](Block *block)
    {
      Block::iterator begin;
      bool defined = false;
      if (auto it = splits.find(block); it != splits.end()) {
        // Find out if the value is live-out of the placement point.
        // Register the value, if defined in block.
        if (liveMov.count(block)) {
          auto arg = defs.empty() ? ref : defs.top()->GetSubValue(0);
          auto *mov = new MovInst(ToType(ref.GetType(), it->second), arg, {});
          block->insert(mov, block->first_non_phi());
          defs.push(mov);
          newMovs.emplace(mov, it->second);
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
      for (auto *child : *doms.DT[block]) {
        rewrite(child->getBlock());
      }
      // Remove the definition.
      if (defined) {
        defs.pop();
      }
    };
  rewrite(doms.DT.getRoot());

  // Recompute the types of the users of the refined instructions.
  for (auto &[mov, type] : newMovs) {
    assert(mov->use_size() > 0 && "dead mov");
    analysis_.Define(mov->GetSubValue(0), type);
  }
  // Schedule the PHIs to be recomputed.
  for (auto &[phi, type] : newPhis) {
    assert(phi->use_size() > 0 && "dead phi");
    analysis_.Define(phi->GetSubValue(0), type);
  }
  // Trigger an update of anything relying on the reference.
  analysis_.Refine(ref, analysis_.Find(ref));
}

// -----------------------------------------------------------------------------
void Refinement::PullFrontier()
{
  for (Block *block : llvm::post_order(&func_)) {
    using BranchMap = std::unordered_map<Ref<Inst>, TaggedType>;
    std::optional<BranchMap> merges;
    for (auto *succ : block->successors()) {
      BranchMap branch;
      for (auto it = succ->first_non_phi(); it != succ->end(); ++it) {
        if (auto *mov = ::cast_or_null<MovInst>(&*it)) {
          if (auto inst = ::cast_or_null<Inst>(mov->GetArg())) {
            if (mov->GetType() == inst.GetType() && inst->getParent() != succ) {
              branch.emplace(inst, analysis_.Find(mov->GetSubValue(0)));
            }
          }
        } else {
          break;
        }
      }

      if (!merges.has_value()) {
        merges.emplace(std::move(branch));
      } else {
        for (auto it = merges->begin(); it != merges->end(); ) {
          auto jt = branch.find(it->first);
          if (jt == branch.end() || jt->second != it->second) {
            it = merges->erase(it);
          } else {
            ++it;
          }
        }
      }
    }

    if (merges.has_value() && !merges->empty()) {
      for (auto &[ref, ty] : *merges) {
        if (analysis_.Find(ref) != ty) {
          Refine(block, ref, ty);
        }
      }
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
  if (vo.IsPtrLike() && vl.IsPtrUnion() && vr.IsInt()) {
    RefineAddr(i, i.GetLHS());
    return;
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitAddInst(AddInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
  if (vo.IsPtrLike()) {
    if (vl.IsInt() && vr.IsPtrUnion()) {
      RefineAddr(i, i.GetRHS());
    }
    if (vr.IsInt() && vl.IsPtrUnion()) {
      RefineAddr(i, i.GetLHS());
    }
    if (vl.IsPtrLike() && vr.IsPtrUnion()) {
      RefineInt(i, i.GetRHS());
    }
    if (vr.IsPtrLike() && vl.IsPtrUnion()) {
      RefineInt(i, i.GetLHS());
    }
  }
  if (vo.IsVal()) {
    // addr|int + int == addr | int, so if addr|int + int = val, then
    // the address and the val must be integers.
    if (vl.IsAddrInt() && vr.IsInt()) {
      RefineInt(i, i.GetSubValue(0));
      RefineInt(i, i.GetLHS());
    }
    if (vr.IsAddrInt() && vl.IsInt()) {
      RefineInt(i, i.GetSubValue(0));
      RefineInt(i, i.GetRHS());
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitCmpInst(CmpInst &i)
{
  auto cc = i.GetCC();
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());

  if (vl.IsAddrInt() && vr.IsInt()) {
    return Refine(i.getParent(), i.GetLHS(), TaggedType::Int());
  }
  if (vr.IsAddrInt() && vl.IsInt()) {
    return Refine(i.getParent(), i.GetRHS(), TaggedType::Int());
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
  auto inst = ::cast_or_null<Inst>(i.GetArg());
  if (!inst) {
    return;
  }

  auto *parent = i.getParent();
  if (parent != inst->getParent()) {
    auto &doms = analysis_.GetDoms(*parent->getParent());
    auto *node = doms.PDT.getNode(parent);
    if (!node) {
      return;
    }
    // Split through specialisation.
    auto *parent = i.getParent();
    if (parent != inst->getParent()) {
      for (Block *pred : parent->predecessors()) {
        if (pred->succ_size() != 1) {
          return;
        }
      }
    }
    // Split through refinement.
    bool isSplit = false;
    for (auto *front : doms.PDF.calculate(doms.PDT, node)) {
      if (front == parent) {
        isSplit = true;
        break;
      }
      for (auto *succ : front->successors()) {
        if (i.getParent()->getName() == ".L180$local94132") {
          llvm::errs() << "\t" << succ->getName() << "\n";
        }
        if (succ == parent) {
          isSplit = true;
          break;
        }
      }
      if (isSplit) {
        break;
      }
    }
    if (isSplit) {
      return;
    }
  }

  auto varg = analysis_.Find(inst);
  auto vmov = analysis_.Find(i.GetSubValue(0));
  if (auto nt = RefineMovTo(vmov, varg, i.GetType())) {
    NumMovsRefined++;

    if (i.getParent()->getName() == ".L180$local94132") {
      if (nt->IsOne()) {
        llvm::errs() << *inst << "\n";
        analysis_.dump();
        abort();
      }
    }

    Refine(parent, inst, *nt);
    return;
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitCallSite(CallSite &site)
{
  auto callee = analysis_.Find(site.GetCallee());
  if (callee.IsUnknown()) {
    return;
  }

  // Refine the callee to a pointer.
  RefineFunc(site, site.GetCallee());
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
        return RefineEquality(l, r, inst->getParent(), bt, bf);
      }
      case Cond::NE: case Cond::UNE: case Cond::ONE: {
        return RefineEquality(l, r, inst->getParent(), bf, bt);
      }
      case Cond::LE: case Cond::ULE: case Cond::OLE:
      case Cond::LT: case Cond::ULT: case Cond::OLT: {
        return RefineInequality(l, r, inst->getParent(), bt, bf);
      }
      case Cond::GE: case Cond::UGE: case Cond::OGE:
      case Cond::GT: case Cond::UGT: case Cond::OGT: {
        return RefineInequality(l, r, inst->getParent(), bf, bt);
      }
      default: {
        return;
      }
    }
    llvm_unreachable("invalid condition code");
  }
  if (auto inst = ::cast_or_null<AndInst>(jcc.GetCond())) {
    if (analysis_.Find(jcc.GetCond()).IsZeroOrOne()) {
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
Ref<Inst> Refinement::Cast(Ref<Inst> ref, const TaggedType &ty)
{
  auto *newMov = new MovInst(ToType(ref.GetType(), ty), ref, ref->GetAnnots());
  ref->getParent()->insertAfter(newMov, ref->getIterator());
  return newMov;
}

// -----------------------------------------------------------------------------
void Refinement::RefineJoin(
    Ref<Inst> ref,
    const TaggedType &ty,
    Use &use,
    Type type)
{
  assert((*use) == ref && "invalid use");

  auto vref = analysis_.Find(ref);
  Inst *user = ::cast<Inst>(use.getUser());
  if (auto nt = RefineJoinTo(vref, ty, type)) {
    if (nt->second) {
      auto newRef = Cast(ref, nt->first);
      use = newRef;
      analysis_.Define(newRef, ty);
    } else {
      Refine(user->getParent(), ref, nt->first);
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
  // Since the PHI can change during the traversal, collect all blocks.
  llvm::SmallVector<Block *, 8> blocks;
  for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
    blocks.push_back(phi.GetBlock(i));
  }
  // Attempt to refine all incoming values.
  auto *parent = phi.getParent();
  auto *func = parent->getParent();
  for (Block *block : blocks) {
    auto ref = phi.GetValue(block);
    auto vref = analysis_.Find(ref);
    if (auto nt = RefineJoinTo(vref, vphi, phi.GetType())) {
      if (nt->second) {
        auto newRef = Cast(ref, nt->first);
        phi.Remove(block);
        phi.Add(block, newRef);
        analysis_.Define(newRef, nt->first);
      } else {
        Refine(block, parent, ref, nt->first);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitArgInst(ArgInst &arg)
{
  auto ty = analysis_.Find(arg);
  if (ty.IsUnknown()) {
    return;
  }
  auto i = arg.GetIndex();
  Func *f = arg.getParent()->getParent();
  for (auto *user : f->users()) {
    auto mov = ::cast_or_null<MovInst>(user);
    if (!mov) {
      continue;
    }
    for (auto *movUser : mov->users()) {
      auto call = ::cast_or_null<CallSite>(movUser);
      if (!call || call->GetCallee() != mov->GetSubValue(0)) {
        continue;
      }
      if (call->arg_size() > i) {
        RefineJoin(call->arg(i), ty, *(call->op_begin() + 1 + i), arg.GetType());
      }
    }
  }
}

// -----------------------------------------------------------------------------
void Refinement::VisitSelectInst(SelectInst &i)
{
  auto vo = analysis_.Find(i);
  auto ty = i.GetType();
  RefineJoin(i.GetTrue(), vo, *(i.op_begin() + 1), ty);
  RefineJoin(i.GetFalse(), vo, *(i.op_begin() + 2), ty);
}
