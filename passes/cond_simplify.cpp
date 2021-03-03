// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>

#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/inst_visitor.h"
#include "core/inst_compare.h"
#include "core/analysis/dominator.h"
#include "passes/cond_simplify.h"

#define DEBUG_TYPE "cond-simplify"

STATISTIC(NumCondsSimplified, "Conditions simplified");
STATISTIC(NumPtrChecksSimplified, "Pointer checks simplified");



// -----------------------------------------------------------------------------
const char *CondSimplifyPass::kPassID = "cond-simplify";

// -----------------------------------------------------------------------------
static bool IsDominatorEdge(DominatorTree &dt, Block *start, Block *end)
{
  if (end->pred_size() == 1) {
    return true;
  }

  int isDuplicateEdge = 0;
  for (Block *pred : end->predecessors()) {
    if (pred == start) {
      if (isDuplicateEdge++) {
        return false;
      }
      continue;
    }
    if (!dt.dominates(end, pred)) {
      return false;
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
union Condition {
  enum class Kind {
    JUMP,
    SWITCH,
  };

  struct JumpCond {
    Kind K;
    ConstRef<Inst> Arg;
    bool Flag;

    JumpCond(ConstRef<Inst> arg, bool flag)
      : K(Kind::JUMP)
      , Arg(arg)
      , Flag(flag)
    {
    }
  };

  struct SwitchCond {
    Kind K;
    ConstRef<Inst> Arg;
    unsigned Index;

    SwitchCond(ConstRef<Inst> arg, unsigned index)
      : K(Kind::SWITCH)
      , Arg(arg)
      , Index(index)
    {
    }
  };

  Kind K;
  JumpCond Jump;
  SwitchCond Switch;

  Condition(ConstRef<Inst> cond, bool flag) : Jump(cond, flag) {}
  Condition(ConstRef<Inst> arg, unsigned index) : Jump(arg, index) {}
};

// -----------------------------------------------------------------------------
static bool IsEqual(ConstRef<Inst> a, ConstRef<Inst> b)
{
  if (a == b) {
    return true;
  }
  if (a->IsConstant() && b->IsConstant()) {
    return InstCompare().IsEqual(*a.Get(), *b.Get());
  }
  return false;
}

// -----------------------------------------------------------------------------
static std::optional<int64_t> GetConstant(Ref<Inst> inst)
{
  if (auto movInst = ::cast_or_null<MovInst>(inst)) {
    if (auto movValue = ::cast_or_null<ConstantInt>(movInst->GetArg())) {
      if (movValue->GetValue().getMinSignedBits() <= 64) {
        return movValue->GetInt();
      }
    }
  }
  return {};
}

// -----------------------------------------------------------------------------
class CondSimplifier : public InstVisitor<bool> {
public:
  CondSimplifier(Func &func) : func_(func), dt_(func) {}

  bool VisitInst(Inst &) { return false; }

  bool VisitAddInst(AddInst &add)
  {
    if (pointers_.count(add.GetLHS()) || pointers_.count(add.GetRHS())) {
      pointers_.emplace(&add);
    }
    return false;
  }

  bool VisitCmpInst(CmpInst &cmp)
  {
    const Cond cc = cmp.GetCC();
    const Type ty = cmp.GetType();

    // Try to simplify null pointer checks.
    if (cc == Cond::EQ) {
      if (auto flag = IsNonNull(cmp.GetLHS(), cmp.GetRHS())) {
        Rewrite(cmp, !*flag);
        NumPtrChecksSimplified++;
        return true;
      }
      if (auto flag = IsNonNull(cmp.GetRHS(), cmp.GetLHS())) {
        Rewrite(cmp, !*flag);
        NumPtrChecksSimplified++;
        return true;
      }
    }
    if (cc == Cond::NE) {
      if (auto flag = IsNonNull(cmp.GetLHS(), cmp.GetRHS())) {
        Rewrite(cmp, *flag);
        NumPtrChecksSimplified++;
        return true;
      }
      if (auto flag = IsNonNull(cmp.GetRHS(), cmp.GetLHS())) {
        Rewrite(cmp, *flag);
        NumPtrChecksSimplified++;
        return true;
      }
    }

    // Try to simplify based on dominating edges.
    for (const auto &cond : conds_) {
      if (auto flag = IsRedundant(cmp, cond)) {
        Rewrite(cmp, *flag);
        NumCondsSimplified++;
        return true;
      }
    }
    return false;
  }

  void Rewrite(OperatorInst &inst, bool flag)
  {
    Inst *value = new MovInst(inst.GetType(), new ConstantInt(flag), {});
    inst.getParent()->AddInst(value, &inst);
    inst.replaceAllUsesWith(value);
  }

  std::optional<bool> IsNonNull(Ref<Inst> ptrCand, Ref<Inst> zeroCand)
  {
    auto val = GetConstant(zeroCand);
    if (!val || *val != 0) {
      return std::nullopt;
    }
    if (pointers_.count(ptrCand)) {
      #ifndef NDEBUG
      auto ty = ptrCand.GetType();
      assert(ty == Type::I64 || ty == Type::V64 && "not a pointer");
      #endif
      return true;
    }
    return std::nullopt;
  }

  std::optional<bool> IsRedundant(CmpInst &cmp, const Condition &cond)
  {
    switch (cond.K) {
      case Condition::Kind::JUMP: {
        if (auto prior = ::cast_or_null<CmpInst>(cond.Jump.Arg)) {
          const Cond priorCC = prior->GetCC();
          const bool sameLHS = IsEqual(cmp.GetLHS(), prior->GetLHS());
          const bool sameRHS = IsEqual(cmp.GetRHS(), prior->GetRHS());
          if (sameLHS && sameRHS) {
            Inst *value = nullptr;
            if (cmp.GetCC() == priorCC) {
              return cond.Jump.Flag;
            }
            if (cmp.GetCC() == GetInverseCond(priorCC)) {
              return !cond.Jump.Flag;
            }
          }
        }
        // TODO
        return std::nullopt;
      }
      case Condition::Kind::SWITCH: {
        // TODO
        return std::nullopt;
      }
    }
    llvm_unreachable("invalid condition kind");
  }

  bool Traverse(Block &block)
  {
    blocks_.insert(&block);
    auto restore = conds_.size();
    std::unordered_set<Ref<Inst>> pointers = pointers_;

    // Find new dominating edges ending at the current node.
    for (Block *start : block.predecessors()) {
      if (IsDominatorEdge(dt_, start, &block)) {
        auto *term = start->GetTerminator();
        switch (term->GetKind()) {
          default: llvm_unreachable("not a terminator");
          case Inst::Kind::JUMP:
          case Inst::Kind::TRAP:
          case Inst::Kind::CALL:
          case Inst::Kind::TAIL_CALL:
          case Inst::Kind::INVOKE:
          case Inst::Kind::RAISE: {
            break;
          }
          case Inst::Kind::JUMP_COND: {
            auto *jcc = static_cast<JumpCondInst *>(term);
            if (&block == jcc->GetTrueTarget()) {
              conds_.emplace_back(jcc->GetCond(), true);
            } else if (&block == jcc->GetFalseTarget()) {
              conds_.emplace_back(jcc->GetCond(), false);
            } else {
              llvm_unreachable("invalid jump");
            }
            break;
          }
          case Inst::Kind::SWITCH: {
            auto *sw = static_cast<SwitchInst *>(term);
            std::optional<unsigned> index;
            for (unsigned i = 0, n = sw->getNumSuccessors(); i < n; ++i) {
              if (sw->getSuccessor(i) == &block) {
                index = i;
                break;
              }
            }
            if (index) {
              conds_.emplace_back(sw->GetIndex(), *index);
            } else {
              llvm_unreachable("invalid switch");
            }
            break;
          }
        }
      }
    }

    bool changed = false;
    for (auto it = block.begin(); it != block.end(); ) {
      Inst &inst = *it++;
      // Try to infer which instructions generate pointers.
      Abduct(inst);
      // Simplify instructions based on information from all
      // instructions or edges which dominate them.
      changed = Dispatch(inst) || changed;
    }

    if (auto *node = dt_[&block]) {
      for (auto *child : *node) {
        if (auto *childBlock = child->getBlock()) {
          changed = Traverse(*childBlock) || changed;
        }
      }
    }

    conds_.erase(conds_.begin() + restore, conds_.end());
    blocks_.erase(&block);
    pointers_ = pointers;
    return changed;
  }

  void Abduct(Inst &inst)
  {
    if (auto *load = ::cast_or_null<MemoryLoadInst>(&inst)) {
      Abduct(load->GetAddr());
      return;
    }
    if (auto *store = ::cast_or_null<MemoryStoreInst>(&inst)) {
      Abduct(store->GetAddr());
      return;
    }
    if (auto *xchg = ::cast_or_null<MemoryExchangeInst>(&inst)) {
      Abduct(xchg->GetAddr());
      return;
    }
  }

  void Abduct(Ref<Inst> addr)
  {
    if (!blocks_.count(addr->getParent())) {
      return;
    }
    pointers_.insert(addr);

    if (auto add = ::cast_or_null<AddInst>(addr)) {
      if (GetConstant(add->GetLHS())) {
        return Abduct(add->GetRHS());
      }
      if (GetConstant(add->GetRHS())) {
        return Abduct(add->GetLHS());
      }
      return;
    }
    if (auto sub = ::cast_or_null<SubInst>(addr)) {
      if (GetConstant(sub->GetLHS())) {
        return Abduct(sub->GetRHS());
      }
      if (GetConstant(sub->GetRHS())) {
        return Abduct(sub->GetLHS());
      }
      return;
    }
    if (auto select = ::cast_or_null<SelectInst>(addr)) {
      Abduct(select->GetFalse());
      Abduct(select->GetTrue());
    }
  }

private:
  /// Function to simplify.
  Func &func_;
  /// Dominator tree.
  DominatorTree dt_;
  /// Stack of conditions.
  std::vector<Condition> conds_;
  /// Stack of dominators.
  std::set<Block *> blocks_;
  /// Stack of instructions assumed to be pointers.
  std::unordered_set<Ref<Inst>> pointers_;
};

// -----------------------------------------------------------------------------
bool CondSimplifyPass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    changed = CondSimplifier(func).Traverse(func.getEntryBlock()) || changed;
  }
  return changed;
}


// -----------------------------------------------------------------------------
const char *CondSimplifyPass::GetPassName() const
{
  return "Redundant Condition Elimination";
}
