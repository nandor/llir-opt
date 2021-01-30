// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

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



// -----------------------------------------------------------------------------
const char *CondSimplifyPass::kPassID = "cond-simplify";

// -----------------------------------------------------------------------------
bool CondSimplifyPass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    changed = Run(func) || changed;
  }
  return changed;
}

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
class CondSimplifier : public InstVisitor<bool> {
public:
  CondSimplifier(const std::vector<Condition> &conds) : conds_(conds) {}

  bool VisitInst(Inst &) { return false; }

  bool VisitCmpInst(CmpInst &cmp)
  {
    const Cond cc = cmp.GetCC();
    const Type ty = cmp.GetType();
    for (const auto &cond : conds_) {
      switch (cond.K) {
        case Condition::Kind::JUMP: {
          if (auto prior = ::cast_or_null<CmpInst>(cond.Jump.Arg)) {
            const Cond priorCC = prior->GetCC();
            const bool sameLHS = IsEqual(cmp.GetLHS(), prior->GetLHS());
            const bool sameRHS = IsEqual(cmp.GetRHS(), prior->GetRHS());
            if (sameLHS && sameRHS) {
              Inst *value = nullptr;
              if (cc == priorCC) {
                value = new MovInst(ty, new ConstantInt(cond.Jump.Flag), {});
              }
              if (cc == GetInverseCond(priorCC)) {
                value = new MovInst(ty, new ConstantInt(!cond.Jump.Flag), {});
              }
              if (value) {
                cmp.getParent()->AddInst(value, &cmp);
                cmp.replaceAllUsesWith(value);
                NumCondsSimplified++;
                return true;
              }
            }
          }
          // TODO
          continue;
        }
        case Condition::Kind::SWITCH: {
          // TODO
          continue;
        }
      }
      llvm_unreachable("invalid condition kind");
    }
    return false;
  }

private:
  const std::vector<Condition> &conds_;
};

// -----------------------------------------------------------------------------
bool CondSimplifyPass::Run(Func &func)
{
  DominatorTree dt(func);

  std::vector<Condition> conds;
  std::function<bool (Block &)> traverse = [&] (Block &block) -> bool
    {
      auto restore = conds.size();

      // Find new dominating edges ending at the current node.
      for (Block *start : block.predecessors()) {
        if (IsDominatorEdge(dt, start, &block)) {
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
                conds.emplace_back(jcc->GetCond(), true);
              } else if (&block == jcc->GetFalseTarget()) {
                conds.emplace_back(jcc->GetCond(), false);
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
                conds.emplace_back(sw->GetIndex(), *index);
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
        changed = CondSimplifier(conds).Dispatch(*it++) || changed;
      }

      if (auto *node = dt[&block]) {
        for (auto *child : *node) {
          if (auto *childBlock = child->getBlock()) {
            changed = traverse(*childBlock) || changed;
          }
        }
      }

      conds.erase(conds.begin() + restore, conds.end());
      return changed;
    };
  return traverse(func.getEntryBlock());
}

// -----------------------------------------------------------------------------
const char *CondSimplifyPass::GetPassName() const
{
  return "Redundant Condition Elimination";
}
