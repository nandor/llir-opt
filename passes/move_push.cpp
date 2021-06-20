// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/Statistic.h>

#include "core/analysis/dominator.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/clone.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/move_push.h"



#define DEBUG_TYPE "move-push"

STATISTIC(NumMovsRewritten, "Number of types rewritten");


// -----------------------------------------------------------------------------
const char *MovePushPass::kPassID = "move-push";

// -----------------------------------------------------------------------------
namespace {
class TypeRewriter final : public CloneVisitor {
public:
  TypeRewriter(Ref<Inst> ref, Type ty) : ref_(ref), ty_(ty) {}

  ~TypeRewriter() { Fixup(); }

  Type Map(Type ty, Inst *inst, unsigned idx) override
  {
    assert(inst == &*ref_ && "invalid instruction");
    return idx == ref_.Index() ? ty_ : ty;
  }

private:
  /// Sub-value to rewrite.
  Ref<Inst> ref_;
  /// Type to change to.
  Type ty_;
};
}

// -----------------------------------------------------------------------------
bool MovePushPass::Run(Prog &prog)
{
  bool changed = false;
  for (auto &func : prog) {
    std::unique_ptr<PostDominatorTree> pdt = nullptr;
    for (auto *block : llvm::ReversePostOrderTraversal<Func*>(&func)) {
      for (auto it = block->begin(); it != block->end(); ) {
        auto *mov = ::cast_or_null<MovInst>(&*it++);
        if (!mov) {
          continue;
        }
        Ref<Inst> arg = ::cast_or_null<Inst>(mov->GetArg());
        if (!arg || mov->GetType() != Type::V64 || arg.GetType() != Type::I64) {
          continue;
        }
        if (!pdt) {
          pdt.reset(new PostDominatorTree(func));
        }
        if (!pdt->dominates(block, arg->getParent())) {
          continue;
        }
        auto newInst = TypeRewriter(arg, Type::V64).Clone(&*arg);
        arg->getParent()->AddInst(newInst, &*arg);
        arg->replaceAllUsesWith(newInst);
        mov->replaceAllUsesWith(newInst->GetSubValue(arg.Index()));
        arg->eraseFromParent();
        mov->eraseFromParent();
        changed = true;
        ++NumMovsRewritten;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *MovePushPass::GetPassName() const
{
  return "Move Type Rewriting";
}
