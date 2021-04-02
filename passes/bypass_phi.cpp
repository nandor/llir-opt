// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_map>
#include <stack>

#include <llvm/ADT/Statistic.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/clone.h"
#include "core/analysis/dominator.h"
#include "passes/bypass_phi.h"

#define DEBUG_TYPE "bypass-phi"

STATISTIC(NumPhisBypassed, "PHIs bypassed");



// -----------------------------------------------------------------------------
const char *BypassPhiPass::kPassID = "simplify-cfg";

// -----------------------------------------------------------------------------
bool BypassPhiPass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    if (func.getName() != "secp256k1_ge_globalz_set_table_gej") continue;

    bool iter = false;
    do {
      iter = false;
      for (Block &block : func) {
        if (BypassPhiCmp(block)) {
          iter = true;
          break;
        }
      }
      changed = changed || iter;
    } while (iter);
  }

  return changed;
}

// -----------------------------------------------------------------------------
const char *BypassPhiPass::GetPassName() const
{
  return "Control Flow Simplification";
}

// -----------------------------------------------------------------------------
namespace {
class Cloner final : public CloneVisitor {
public:
  Cloner(Block *from, Block *to) : from_(from), to_(to) {}

  Block *Map(Block *block)
  {
    return block == from_ ? to_ : block;
  }

private:
  /// Original block.
  Block *from_;
  /// Block to rewrite with.
  Block *to_;
};
} // namespace


// -----------------------------------------------------------------------------
bool BypassPhiPass::Bypass(
    JumpCondInst &jcc,
    CmpInst &cmp,
    Ref<Inst> phiCandidate,
    Ref<Inst> reference,
    Block &block)
{
  Func &f = *block.getParent();

  // Find a set of nodes satisfying the following structure:
  //
  //          pred   /           pred     /
  //            \   /              |     /
  //             \ /               |    /
  //            block    =>        \  block
  //   \         / \         \      \  / \
  //    \       /   \         \      phi  \
  //     \     /               \     /
  //      target               target
  //        |                    |
  //
  // The transformation bypasses the pred-block-target chain
  // with a more direct pred-target jump.

  auto phi = ::cast_or_null<PhiInst>(phiCandidate);
  if (!phi || phi->getParent() != &block) {
    return false;
  }

  Block *pred = nullptr;
  for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
    if (phi->GetValue(i) == reference) {
      pred = phi->GetBlock(i);
      break;
    }
  }
  if (!pred) {
    return false;
  }

  Block *target = nullptr;
  if (cmp.GetCC() == Cond::EQ) {
    target = jcc.GetTrueTarget();
  } else if (cmp.GetCC() == Cond::NE) {
    target = jcc.GetFalseTarget();
  } else {
    return false;
  }

  // Find the set of nodes dominated by the original block.
  std::set<Block *> dominatedByBlock;
  {
    DominatorTree DT(f);
    std::function<void(Block *)> traverse = [&](Block *b)
    {
      dominatedByBlock.insert(b);
      for (const auto *child : *DT[b]) {
        traverse(child->getBlock());
      }
    };
    traverse(&block);
  }

  // Bypass the block from pred to target.
  Block *phiPlace = nullptr;
  if (target->pred_size() == 1) {
    phiPlace = target;
    auto *predTerm = pred->GetTerminator();
    auto *newPredTerm = Cloner(&block, target).Clone(predTerm);
    pred->AddInst(newPredTerm);
    predTerm->replaceAllUsesWith(newPredTerm);
    predTerm->eraseFromParent();
  } else {
    auto *join = new Block(target->getName());
    target->getParent()->AddBlock(join, target);
    join->AddInst(new JumpInst(target, {}));

    auto *predTerm = pred->GetTerminator();
    auto *newPredTerm = Cloner(&block, join).Clone(predTerm);
    pred->AddInst(newPredTerm);
    predTerm->replaceAllUsesWith(newPredTerm);
    predTerm->eraseFromParent();

    auto *blockTerm = block.GetTerminator();
    auto *newBlockTerm = Cloner(target, join).Clone(blockTerm);
    block.AddInst(newBlockTerm);
    blockTerm->replaceAllUsesWith(newBlockTerm);
    blockTerm->eraseFromParent();

    phiPlace = join;
    for (PhiInst &phi : target->phis()) {
      auto value = phi.GetValue(&block);
      phi.Remove(&block);
      phi.Add(join, value);
    }
  }

  if (!block.phi_empty()) {
    std::vector<std::pair<PhiInst *, PhiInst *>> phis;
    for (PhiInst &phi : block.phis()) {
      auto value = phi.GetValue(pred);
      phi.Remove(pred);

      auto *newPhi = new PhiInst(phi.GetType(), phi.GetAnnots());
      newPhi->Add(&block, &phi);
      newPhi->Add(pred, value);
      phiPlace->AddPhi(newPhi);

      phis.emplace_back(&phi, newPhi);
    }

    DominatorTree DT(f);
    DominanceFrontier DF;
    DF.analyze(DT);

    std::unordered_map<PhiInst *, PhiInst *> newPhis;
    for (auto [oldPhi, newPhi] : phis) {
      newPhis.emplace(newPhi, oldPhi);
      newPhis.emplace(oldPhi, oldPhi);
    }

    std::queue<Block *> q;
    q.emplace(target);
    while (!q.empty()) {
      Block *b = q.front();
      q.pop();

      bool found = false;
      for (PhiInst &phi : b->phis()) {
        if (newPhis.count(&phi)) {
          found = true;
          break;
        }
      }
      if (!found) {
        for (auto [oldPhi, tgtPhi] : phis) {
          auto *newPhi = new PhiInst(oldPhi->GetType(), {});
          b->AddPhi(newPhi);
          newPhis.emplace(newPhi, oldPhi);
        }
        for (auto *front : DF.calculate(DT, DT.getNode(b))) {
          if (!dominatedByBlock.count(front)) {
            continue;
          }
          q.push(front);
        }
      }
    }

    std::unordered_map<PhiInst *, std::stack<PhiInst *>> defs;
    std::function<void(Block *)> rename = [&](Block *b)
    {
      // Register the names of incoming PHIs.
      for (PhiInst &phi : b->phis()) {
        if (b == &block) {
          defs[&phi].push(&phi);
        } else {
          auto it = newPhis.find(&phi);
          if (it != newPhis.end()) {
            defs[it->second].push(&phi);
          }
        }
      }
      // Rewrite all uses in this block.
      for (Inst &inst : *b) {
        if (inst.Is(Inst::Kind::PHI)) {
          continue;
        }
        for (Use &use : inst.operands()) {
          if (auto *phiUse = ::cast_or_null<PhiInst>(use.get().Get())) {
            auto it = defs.find(phiUse);
            if (it != defs.end()) {
              assert(!it->second.empty());
              use = it->second.top();
            }
          }
        }
      }
      // Handle PHI nodes in successors.
      for (Block *succ : b->successors()) {
        if (succ == phiPlace || succ == &block) {
          continue;
        }
        for (PhiInst &phi : succ->phis()) {
          auto phiIt = newPhis.find(&phi);
          if (phiIt != newPhis.end()) {
            auto defIt = defs.find(phiIt->second);
            assert(defIt != defs.end());
            assert(!defIt->second.empty());
            phi.Add(b, defIt->second.top());
          } else {
            if (auto phiUse = ::cast_or_null<PhiInst>(phi.GetValue(b))) {
              auto it = defs.find(phiUse.Get());
              if (it != defs.end()) {
                assert(!it->second.empty());
                phi.Remove(b);
                phi.Add(b, it->second.top());
              }
            }
          }
        }
      }
      // Recursively rename child nodes.
      for (const auto *child : *DT[b]) {
        rename(child->getBlock());
      }

      // Pop definitions of this block from the stack.
      for (PhiInst &phi : b->phis()) {
        if (b == &block) {
          defs[&phi].pop();
        } else {
          auto it = newPhis.find(&phi);
          if (it != newPhis.end()) {
            defs[it->second].pop();
          }
        }
      }
    };
    rename(&f.getEntryBlock());
  }

  NumPhisBypassed++;
  return true;
}

// -----------------------------------------------------------------------------
bool BypassPhiPass::BypassPhiCmp(Block &block)
{
  if (auto *jcc = ::cast_or_null<JumpCondInst>(block.GetTerminator())) {
    CmpInst *cmp = nullptr;
    for (auto it = block.begin(); std::next(it) != block.end(); ++it) {
      auto *phi = ::cast_or_null<PhiInst>(&*it);
      if (phi) {
        continue;
      }
      auto *singleCmp = ::cast_or_null<CmpInst>(&*it);
      if (singleCmp) {
        if (cmp) {
          return false;
        } else {
          cmp = singleCmp;
        }
      } else {
        return false;
      }
    }
    if (!cmp || cmp->use_size() != 1) {
      return false;
    }
    if (Bypass(*jcc, *cmp, cmp->GetLHS(), cmp->GetRHS(), block)) {
      return true;
    }
    if (Bypass(*jcc, *cmp, cmp->GetRHS(), cmp->GetLHS(), block)) {
      return true;
    }
    return false;
  }
  return false;
}
