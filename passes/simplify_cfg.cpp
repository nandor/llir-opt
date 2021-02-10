// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/clone.h"
#include "core/analysis/dominator.h"
#include "passes/simplify_cfg.h"

#define DEBUG_TYPE "simplify-cfg"

STATISTIC(NumCondJumpsEliminated, "Conditional jumps eliminated");
STATISTIC(NumPhiBlocksEliminated, "Phi-only blocks eliminated");
STATISTIC(NumJumpsThreaded, "Jumps threaded");
STATISTIC(NumPhisRemoved, "Trivial phis removed");
STATISTIC(NumBlocksMerges, "Blocks merged");
STATISTIC(NumBranchesFolded, "Branches folded");
STATISTIC(NumPhisBypassed, "PHIs bypassed");



// -----------------------------------------------------------------------------
const char *SimplifyCfgPass::kPassID = "simplify-cfg";

// -----------------------------------------------------------------------------
bool SimplifyCfgPass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    changed = Run(func) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *SimplifyCfgPass::GetPassName() const
{
  return "Control Flow Simplification";
}

// -----------------------------------------------------------------------------
bool SimplifyCfgPass::Run(Func &func)
{
  bool changed = false;
  changed = EliminateConditionalJumps(func) || changed;
  changed = EliminatePhiBlocks(func) || changed;
  changed = ThreadJumps(func) || changed;
  changed = FoldBranches(func) || changed;
  func.RemoveUnreachable();
  for (Block &block : func) {
    changed = BypassPhiCmp(block) || changed;
  }
  changed = RemoveSinglePhis(func) || changed;
  changed = MergeIntoPredecessor(func) || changed;
  return changed;
}

// -----------------------------------------------------------------------------
bool SimplifyCfgPass::EliminateConditionalJumps(Func &func)
{
  bool changed = false;
  for (auto &block : func) {
    if (auto *jc = ::cast_or_null<JumpCondInst>(block.GetTerminator())) {
      if (jc->GetTrueTarget() == jc->GetFalseTarget()) {
        JumpInst *jmp = new JumpInst(jc->GetTrueTarget(), jc->GetAnnots());
        block.AddInst(jmp, jc);
        jc->replaceAllUsesWith(jmp);
        jc->eraseFromParent();
        NumCondJumpsEliminated++;
        changed = true;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool SimplifyCfgPass::EliminatePhiBlocks(Func &func)
{
  bool changed = false;
  for (auto it = func.begin(); it != func.end(); ) {
    Block *block = &*it++;
    if (!block->IsLocal() || block->HasAddressTaken()) {
      continue;
    }
    // Block most end in a no-op jump.
    auto *jmp = ::cast_or_null<JumpInst>(block->GetTerminator());
    if (!jmp) {
      continue;
    }
    auto *cont = jmp->GetTarget();
    if (cont->pred_size() == 1) {
      continue;
    }

    // Block must only have phis.
    auto preTerm = std::next(block->rbegin());
    if (preTerm == block->rend() || !preTerm->Is(Inst::Kind::PHI)) {
      continue;
    }
    // All uses of the PHIs must be in PHIs in the continuation block.
    bool allUsesInCont = true;
    for (PhiInst &phi : block->phis()) {
      for (User *user : phi.users()) {
        auto *contPhi = ::cast_or_null<PhiInst>(user);
        if (!contPhi || contPhi->getParent() != cont) {
          allUsesInCont = false;
          break;
        }
      }
      if (!allUsesInCont) {
        break;
      }
    }
    if (!allUsesInCont) {
      continue;
    }

    for (PhiInst &phi : cont->phis()) {
      auto value = phi.GetValue(block);
      auto phiRef = ::cast_or_null<PhiInst>(value);
      if (!phiRef || phiRef->getParent() != block) {
        for (Block *pred : block->predecessors()) {
          phi.Add(pred, value);
        }
      } else {
        for (unsigned i = 0, n = phiRef->GetNumIncoming(); i < n; ++i) {
          phi.Add(phiRef->GetBlock(i), phiRef->GetValue(i));
        }
      }
      phi.Remove(block);
    }

    block->replaceAllUsesWith(cont);
    block->eraseFromParent();
    NumPhiBlocksEliminated++;
    changed = true;
  }
  return changed;
}

// -----------------------------------------------------------------------------
static Block *FindThread(Block *start, Block *prev, Block **phi, Block *block)
{
  *phi = prev;
  if (block == start) {
    return block;
  }

  if (block->size() != 1) {
    return block;
  }

  if (auto *jmp = ::cast_or_null<JumpInst>(block->GetTerminator())) {
    auto *target = jmp->GetTarget();
    if (target == block) {
      return block;
    }
    return FindThread(start, block, phi, jmp->GetTarget());
  }
  return block;
}

// -----------------------------------------------------------------------------
static Block *Thread(Block *block, Block **pred, Block *original)
{
  auto *target = FindThread(block, block, pred, original);
  if (original == target) {
    return nullptr;
  }
  return target;
}

// -----------------------------------------------------------------------------
static void AddEdge(Block *block, Block *pred, Block *target)
{
  for (PhiInst &phi : target->phis()) {
    if (!phi.HasValue(block) && phi.HasValue(pred)) {
      phi.Add(block, phi.GetValue(pred));
    }
  }
}

// -----------------------------------------------------------------------------
bool SimplifyCfgPass::ThreadJumps(Func &func)
{
  bool changed = false;
  for (auto &block : func) {
    if (auto *term = block.GetTerminator()) {
      Inst *newInst = nullptr;
      if (auto *jc = ::cast_or_null<JumpCondInst>(term)) {
        Ref<Inst> cond = jc->GetCond();
        Block *bt = jc->GetTrueTarget();
        Block *bf = jc->GetFalseTarget();

        Block *predTrue, *predFalse;
        auto *threadedT = Thread(&block, &predTrue, bt);
        if (threadedT) {
          for (auto &phi : bt->phis()) {
            phi.Remove(&block);
          }
        }

        auto *threadedF = Thread(&block, &predFalse, bf);
        if (threadedF) {
          for (auto &phi : bf->phis()) {
            phi.Remove(&block);
          }
        }

        if (threadedT || threadedF) {
          auto *newT = threadedT ? threadedT : bt;
          auto *newF = threadedF ? threadedF : bf;
          if (newT != newF) {
            if (bt != newT) {
              AddEdge(&block, predTrue, newT);
            }
            if (bf != newF) {
              AddEdge(&block, predFalse, newF);
            }
            newInst = new JumpCondInst(cond, newT, newF, jc->GetAnnots());
          } else {
            for (PhiInst &phi : newT->phis()) {
              if (predTrue != predFalse) {
                auto *select = new SelectInst(
                    phi.GetType(),
                    cond,
                    phi.GetValue(predTrue),
                    phi.GetValue(predFalse),
                    phi.GetAnnots()
                );
                block.AddInst(select, jc);
                phi.Add(&block, select);
              } else {
                phi.Add(&block, phi.GetValue(predTrue));
              }
            }
            newInst = new JumpInst(newT, jc->GetAnnots());
          }
        }
      }

      if (auto *jmp = ::cast_or_null<JumpInst>(term)) {
        Block *pred;
        if (auto *target = Thread(&block, &pred, jmp->GetTarget())) {
          AddEdge(&block, pred, target);
          for (auto &phi : jmp->GetTarget()->phis()) {
            phi.Remove(&block);
          }
          newInst = new JumpInst(target, jmp->GetAnnots());
        }
      }

      if (auto *call = ::cast_or_null<CallInst>(term)) {
        Block *pred;
        if (auto *target = Thread(&block, &pred, call->GetCont())) {
          AddEdge(&block, pred, target);
          for (auto &phi : call->GetCont()->phis()) {
            phi.Remove(&block);
          }
          newInst = new CallInst(
              std::vector<Type>(call->type_begin(), call->type_end()),
              call->GetCallee(),
              std::vector<Ref<Inst>>(call->arg_begin(), call->arg_end()),
              call->GetFlags(),
              target,
              call->GetNumFixedArgs(),
              call->GetCallingConv(),
              call->GetAnnots()
          );
        }
      }

      if (newInst) {
        block.AddInst(newInst, term);
        term->replaceAllUsesWith(newInst);
        term->eraseFromParent();
        NumJumpsThreaded++;
        changed = true;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool SimplifyCfgPass::FoldBranches(Func &func)
{
  bool changed = false;
  for (auto &block : func) {
    if (auto *inst = ::cast_or_null<JumpCondInst>(block.GetTerminator())) {
      bool foldTrue = false;
      bool foldFalse = false;

      if (Ref<MovInst> mov = ::cast_or_null<MovInst>(inst->GetCond())) {
        Value *val = mov->GetArg().Get();
        switch (val->GetKind()) {
          case Value::Kind::INST: {
            continue;
          }
          case Value::Kind::CONST: {
            switch (static_cast<Constant *>(val)->GetKind()) {
              case Constant::Kind::INT: {
                const auto &iv = static_cast<ConstantInt *>(val)->GetValue();
                foldFalse = iv.isNullValue();
                foldTrue = !foldFalse;
                break;
              }
              case Constant::Kind::FLOAT: {
                const auto &fv = static_cast<ConstantFloat *>(val)->GetValue();
                foldFalse = fv.isZero();
                foldTrue = !foldFalse;
                break;
              }
            }
            break;
          }
          case Value::Kind::GLOBAL:
          case Value::Kind::EXPR: {
            foldTrue = true;
            foldFalse = false;
            break;
          }
        }
      }

      Inst *newInst = nullptr;
      if (foldTrue) {
        newInst = new JumpInst(inst->GetTrueTarget(), inst->GetAnnots());
        for (auto &phi : inst->GetFalseTarget()->phis()) {
          phi.Remove(&block);
        }
      }
      if (foldFalse) {
        newInst = new JumpInst(inst->GetFalseTarget(), inst->GetAnnots());
        for (auto &phi : inst->GetTrueTarget()->phis()) {
          phi.Remove(&block);
        }
      }
      if (newInst) {
        block.AddInst(newInst, inst);
        inst->replaceAllUsesWith(newInst);
        inst->eraseFromParent();
        NumBranchesFolded++;
        changed = true;
      }
    }

    if (auto *inst = ::cast_or_null<SwitchInst>(block.GetTerminator())) {
      if (Ref<MovInst> mov = ::cast_or_null<MovInst>(inst->GetIndex())) {
        if (Ref<ConstantInt> val = ::cast_or_null<ConstantInt>(mov->GetArg())) {
          int64_t idx = val->GetValue().getSExtValue();
          unsigned n = inst->getNumSuccessors();

          Inst *newInst;
          if (idx < 0 || n <= idx) {
            newInst = new TrapInst({});
          } else {
            newInst = new JumpInst(inst->getSuccessor(idx), inst->GetAnnots());
          }

          for (unsigned i = 0; i < n; ++i) {
            if (i != idx) {
              Block *block = inst->getSuccessor(i);
              for (auto &phi : block->phis()) {
                phi.Remove(block);
              }
            }
          }

          block.AddInst(newInst, inst);
          inst->replaceAllUsesWith(newInst);
          inst->eraseFromParent();
          NumBranchesFolded++;
          changed = true;
        }
      }
    }
  }

  return changed;
}

// -----------------------------------------------------------------------------
bool SimplifyCfgPass::RemoveSinglePhis(Func &func)
{
  bool changed = false;
  for (auto &block : func) {
    for (auto it = block.begin(); it != block.end(); ) {
      auto *inst = &*it++;
      if (auto *phi = ::cast_or_null<PhiInst>(inst)) {
        if (phi->GetNumIncoming() == 1) {
          phi->replaceAllUsesWith(phi->GetValue(0u));
          phi->eraseFromParent();
          NumPhisRemoved++;
          changed = true;
        }
      } else {
        break;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool SimplifyCfgPass::MergeIntoPredecessor(Func &func)
{
  bool changed = false;
  for (auto bt = ++func.begin(); bt != func.end(); ) {
    // Do not merge multiple blocks with preds.
    auto *block = &*bt++;
    if (block->pred_size() != 1) {
      continue;
    }
    // Do not merge if predecessor diverges.
    Block *pred = *block->pred_begin();
    auto *predJmp = ::cast_or_null<JumpInst>(pred->GetTerminator());
    if (!predJmp || predJmp->GetTarget() != block) {
      continue;
    }
    // Do not merge drops which have the address taken.
    if (block->HasAddressTaken() || !block->IsLocal()) {
      continue;
    }

    // Erase the terminator & transfer all instructions.
    predJmp->eraseFromParent();
    for (auto it = block->begin(); it != block->end(); ) {
      auto *inst = &*it++;
      if (auto *phi = ::cast_or_null<PhiInst>(inst)) {
        assert(phi->GetNumIncoming() == 1 && "invalid phi");
        assert(phi->GetBlock(0u) == pred && "invalid predecessor");
        phi->replaceAllUsesWith(phi->GetValue(0u));
        phi->eraseFromParent();
      } else {
        inst->removeFromParent();
        pred->AddInst(inst);
      }
    }
    block->replaceAllUsesWith(pred);
    block->eraseFromParent();
    NumBlocksMerges++;
    changed = true;
  }
  return changed;
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
static bool Bypass(
    JumpCondInst &jcc,
    CmpInst &cmp,
    Ref<Inst> phiCandidate,
    Ref<Inst> reference,
    Block &block)
{
  Func &f = *block.getParent();

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
    pred->AddInst(Cloner(&block, target).Clone(predTerm));
    predTerm->eraseFromParent();
  } else {
    auto *join = new Block(target->getName());
    target->getParent()->AddBlock(join, target);
    join->AddInst(new JumpInst(target, {}));

    auto *predTerm = pred->GetTerminator();
    pred->AddInst(Cloner(&block, join).Clone(predTerm));
    predTerm->eraseFromParent();

    auto *blockTerm = block.GetTerminator();
    block.AddInst(Cloner(target, join).Clone(blockTerm));
    blockTerm->eraseFromParent();

    phiPlace = join;

    for (PhiInst &phi : target->phis()) {
      auto value = phi.GetValue(&block);
      phi.Remove(&block);
      phi.Add(join, value);
    }
  }

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
  if (!phis.empty()) {
    DominatorTree DT(f);
    DominanceFrontier DF;
    DF.analyze(DT);

    std::unordered_map<PhiInst *, PhiInst *> newPhis;
    for (auto [oldPhi, newPhi] : phis) {
      newPhis.emplace(newPhi, oldPhi);
    }

    std::queue<Block *> q;
    q.emplace(phiPlace);
    while (!q.empty()) {
      Block *b = q.front();
      q.pop();

      for (auto *front : DF.calculate(DT, DT.getNode(b))) {
        if (!dominatedByBlock.count(front)) {
          continue;
        }
        bool found = false;
        for (PhiInst &phi : front->phis()) {
          if (newPhis.count(&phi)) {
            found = true;
            break;
          }
        }
        if (!found) {
          for (auto [oldPhi, tgtPhi] : phis) {
            auto *newPhi = new PhiInst(oldPhi->GetType(), {});
            front->AddPhi(newPhi);
            newPhis.emplace(newPhi, oldPhi);
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
          if (phiIt == newPhis.end()) {
            continue;
          }
          assert(!phi.HasValue(b) && "phi already has value");
          auto defIt = defs.find(phiIt->second);
          assert(defIt != defs.end());
          assert(!defIt->second.empty());
          phi.Add(b, defIt->second.top());
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
    rename(&block.getParent()->getEntryBlock());
  }

  NumPhisBypassed++;
  return true;
}

// -----------------------------------------------------------------------------
bool SimplifyCfgPass::BypassPhiCmp(Block &block)
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
