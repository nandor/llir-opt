// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/inst_compare.h"
#include "core/prog.h"
#include "passes/dedup_block.h"


// -----------------------------------------------------------------------------
const char *DedupBlockPass::kPassID = "dedup-block";

// -----------------------------------------------------------------------------
const char *DedupBlockPass::GetPassName() const
{
  return "Block Deduplication";
}


// -----------------------------------------------------------------------------
bool DedupBlockPass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    changed = Run(func) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool DedupBlockPass::Run(Func &func)
{
  bool changed = false;
  std::vector<Block *> candidates;
  for (auto it = llvm::scc_begin(&func); !it.isAtEnd(); ++it) {
    if (it->size() != 1) {
      continue;
    }

    bool replaced = false;
    Block *b1 = (*it)[0];
    for (Block *b2 : candidates) {
      if (IsEqual(b1, b2)) {
        for (Block *b1succ : b1->successors()) {
          for (PhiInst &phi : b1succ->phis()) {
            phi.Remove(b1);
          }
        }
        auto b1it = b1->begin();
        auto b2it = b2->begin();
        while (b1it != b1->end() && b2it != b2->end()) {
          b1it->replaceAllUsesWith(&*b2it);
          b1it++;
          b2it++;
        }
        assert(b1it == b1->end() && b2it == b2->end() && "unequal blocks");
        b1->replaceAllUsesWith(b2);
        b1->eraseFromParent();
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      candidates.push_back(b1);
    }
    changed = changed || replaced;
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool DedupBlockPass::IsEqual(const Block *b1, const Block *b2)
{
  if (b1->size() != b2->size()) {
    return false;
  }
  if (!b1->IsLocal() || !b2->IsLocal()) {
    return false;
  }
  if (b1->IsLandingPad() || b2->IsLandingPad()) {
    return false;
  }

  // Helper class to compare two instructions.
  class Comparison : public InstCompare {
  public:
    Comparison(InstMap &insts) : insts_(insts) {}

    bool Equal(ConstRef<Inst> a, ConstRef<Inst> b) const override
    {
      if (a.Index() != b.Index()) {
        return false;
      }
      auto it = insts_.find(a.Get());
      return it != insts_.end() && it->second == b.Get();
    }
  private:
    InstMap &insts_;
  };

  // Instruction-by-instruction comparison.
  auto itb1 = b1->begin();
  auto itb2 = b2->begin();
  InstMap insts;
  while (itb1 != b1->end() && itb2 != b2->end()) {
    if (!Comparison(insts).IsEqual(*itb1, *itb2)) {
      return false;
    }
    insts.insert({ &*itb1, &*itb2 });
    ++itb1;
    ++itb2;
  }

  if (itb1 != b1->end() || itb2 != b2->end()) {
    return false;
  }

  for (const User *use : b1->users()) {
    if (auto *phi = ::cast_or_null<const PhiInst>(use)) {
      if (!phi->HasValue(b2)) {
        return false;
      }

      auto pv1 = phi->GetValue(b1);
      auto pv2 = phi->GetValue(b2);
      if (pv1 != pv2) {
        if (pv1.Index() != pv2.Index()) {
          return false;
        }
        auto it = insts.find(pv1.Get());
        if (it == insts.end() || it->second != pv2.Get()) {
          return false;
        }
      }
    }
  }

  return true;
}

