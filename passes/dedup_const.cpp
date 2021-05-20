// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/analysis/dominator.h"
#include "passes/dedup_const.h"



// -----------------------------------------------------------------------------
const char *DedupConstPass::kPassID = "dedup-const";

// -----------------------------------------------------------------------------
static std::optional<int64_t> GetConstant(Inst &inst)
{
  if (auto *movInst = ::cast_or_null<MovInst>(&inst)) {
    if (auto movValue = ::cast_or_null<ConstantInt>(movInst->GetArg())) {
      if (movValue->GetValue().getMinSignedBits() <= 64) {
        return movValue->GetInt();
      }
    }
  }
  return {};
}

// -----------------------------------------------------------------------------
namespace {
class DedupConst {
public:
  DedupConst(Func &func) : doms_(func) {}

  unsigned Visit(Block &block)
  {
    unsigned changed = 0;
    for (auto it = block.begin(); it != block.end(); ) {
      Inst &inst = *it++;
      if (auto *mov = ::cast_or_null<MovInst>(&inst)) {
        if (auto n = GetConstant(*mov)) {
          std::pair<Type, int64_t> key(mov->GetType(), *n);
          auto it = movs_.emplace(key, mov->GetSubValue(0));
          if (!it.second) {
            inst.replaceAllUsesWith(it.first->second);
            inst.eraseFromParent(); 
          }
        }
      }
    }
    
    for (const auto *child : *doms_[&block]) {
      changed += Visit(*child->getBlock());
    }

    for (Inst &inst : block) {
      if (auto n = GetConstant(inst)) {
        auto it = movs_.find(std::make_pair(inst.GetType(0), *n));
        assert(it != movs_.end() && "missing values");
        movs_.erase(it);
      }
    }
    return changed; 
  }

private:
  /// Dominator tree.
  DominatorTree doms_;
  /// Constants available for simplification.
  std::unordered_map<std::pair<Type, int64_t>, Ref<Inst>> movs_;
};
}

// -----------------------------------------------------------------------------
bool DedupConstPass::Run(Prog &prog)
{
  bool changed = false;
  for (auto &func : prog) {
    changed = DedupConst(func).Visit(func.getEntryBlock()) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *DedupConstPass::GetPassName() const
{
  return "Constant Deduplication";
}
