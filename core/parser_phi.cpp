// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>
#include <queue>

#include "core/cast.h"
#include "core/parser.h"
#include "core/block.h"
#include "core/insts.h"
#include "core/analysis/dominator.h"



// -----------------------------------------------------------------------------
llvm::Error Parser::PhiPlacement(Func &func, VRegMap vregs)
{
  // Construct the dominator tree & find dominance frontiers.
  DominatorTree DT(func);
  DominanceFrontier DF;
  DF.analyze(DT);

  // Find all definitions of all variables.
  llvm::DenseSet<unsigned> custom;
  for (Block &block : func) {
    for (PhiInst &inst : block.phis()) {
      for (Use &use : inst.operands()) {
        const auto vreg = reinterpret_cast<uint64_t>(use.get().Get());
        if (vreg & 1) {
          custom.insert(vreg >> 1);
        }
      }
    }
  }

  llvm::DenseMap<unsigned, std::queue<Ref<Inst>>> sites;
  for (Block &block : func) {
    llvm::DenseMap<unsigned, Ref<Inst>> localSites;
    for (Inst &inst : block) {
      for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
        Ref<Inst> ref(&inst, i);
        if (auto it = vregs.find(ref); it != vregs.end()) {
          unsigned vreg = it->second;
          if (custom.count(vreg) == 0) {
            localSites[vreg] = ref;
          }
        }
      }
    }
    for (const auto &site : localSites) {
      sites[site.first].push(site.second);
    }
  }

  // Find the dominance frontier of the blocks where variables are defined.
  // Place PHI nodes at the start of those blocks, continuing with the
  // dominance frontier of those nodes iteratively.
  for (auto &var : sites) {
    auto &q = var.second;
    while (!q.empty()) {
      Ref<Inst> inst = q.front();
      q.pop();
      auto *block = inst->getParent();
      if (auto *node = DT.getNode(block)) {
        for (auto &front : DF.calculate(DT, node)) {
          bool found = false;
          for (PhiInst &phi : front->phis()) {
            if (auto it = vregs.find(&phi); it != vregs.end()) {
              if (it->second == var.first) {
                found = true;
                break;
              }
            }
          }

          // If the PHI node was not added already, add it.
          if (!found) {
            auto *phi = new PhiInst(inst.GetType(), {});
            front->AddPhi(phi);
            vregs[phi] = var.first;
            q.push(phi);
          }
        }
      }
    }
  }

  // Renaming variables to point to definitions or PHI nodes.
  llvm::DenseMap<unsigned, std::stack<Ref<Inst>>> vars;
  llvm::SmallPtrSet<Block *, 8> blocks;
  std::function<llvm::Error(Block *)> rename = [&](Block *block) -> llvm::Error
  {
    // Add the block to the set of visited ones.
    blocks.insert(block);

    // Register the names of incoming PHIs.
    for (PhiInst &phi : block->phis()) {
      auto it = vregs.find(&phi);
      if (it != vregs.end()) {
        vars[it->second].push(&phi);
      }
    }

    // Rename all non-phis, registering them in the map.
    for (Inst &inst : *block) {
      if (inst.Is(Inst::Kind::PHI)) {
        continue;
      }

      for (Use &use : inst.operands()) {
        const auto vreg = reinterpret_cast<uint64_t>(use.get().Get());
        if (vreg & 1) {
          auto &stk = vars[vreg >> 1];
          if (stk.empty()) {
            return MakeError("undefined vreg: " + std::to_string(vreg >> 1));
          }
          use = stk.top();
        }
      }

      for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
        Ref<Inst> ref(&inst, i);
        if (auto it = vregs.find(ref); it != vregs.end()) {
          vars[it->second].push(ref);
        }
      }
    }

    // Handle PHI nodes in successors.
    for (Block *succ : block->successors()) {
      for (PhiInst &phi : succ->phis()) {
        if (phi.HasValue(block)) {
          auto *value = phi.GetValue(block).Get();
          const auto vreg = reinterpret_cast<uint64_t>(value);
          if (vreg & 1) {
            phi.Add(block, vars[vreg >> 1].top());
          }
        } else {
          auto &stk = vars[vregs[&phi]];
          if (!stk.empty()) {
            phi.Add(block, stk.top());
          } else {
            Type type = phi.GetType();
            UndefInst *undef = nullptr;
            for (auto it = block->rbegin(); it != block->rend(); ++it) {
              if (it->Is(Inst::Kind::UNDEF)) {
                UndefInst *inst = static_cast<UndefInst *>(&*it);
                if (inst->GetType() == type) {
                  undef = inst;
                  break;
                }
              }
            }
            if (!undef) {
              undef = new UndefInst(phi.GetType(), {});
              block->AddInst(undef, block->GetTerminator());
            }
            phi.Add(block, undef);
          }
        }
      }
    }

    // Recursively rename child nodes.
    for (const auto *child : *DT[block]) {
      if (auto err = rename(child->getBlock())) {
        return err;
      }
    }

    // Pop definitions of this block from the stack.
    for (auto it = block->rbegin(); it != block->rend(); ++it) {
      for (unsigned i = 0, n = it->GetNumRets(); i < n; ++i) {
        Ref<Inst> ref{&*it, i};
        if (auto jt = vregs.find(ref); jt != vregs.end()) {
          auto &q = vars[jt->second];
          assert(q.top() == ref && "invalid type");
          q.pop();
        }
      }
    }
    return llvm::Error::success();
  };

  if (auto err = rename(DT.getRoot())) {
    return err;
  }

  // Remove blocks which are trivially dead.
  std::vector<PhiInst *> queue;
  std::set<PhiInst *> inQueue;
  for (auto it = func.begin(); it != func.end(); ) {
    Block *block = &*it++;
    if (blocks.count(block) == 0) {
      block->replaceAllUsesWith(new ConstantInt(0));
      block->eraseFromParent();
    } else {
      for (auto &phi : block->phis()) {
        if (inQueue.insert(&phi).second) {
          queue.push_back(&phi);
        }
      }
    }
  }

  // Fix up annotations for PHIs: decide between address and value.
  while (!queue.empty()) {
    PhiInst *phi = queue.back();
    queue.pop_back();
    inQueue.erase(phi);

    bool isValue = false;
    for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
      if (auto inst = ::cast_or_null<Inst>(phi->GetValue(i))) {
        isValue = isValue || inst.GetType() == Type::V64;
      }
    }

    if (!isValue || phi->GetType() == Type::V64) {
      continue;
    }

    PhiInst *newPhi = new PhiInst(Type::V64, phi->GetAnnots());
    for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
      newPhi->Add(phi->GetBlock(i), phi->GetValue(i));
    }
    phi->getParent()->AddInst(newPhi, phi);
    phi->replaceAllUsesWith(newPhi);
    phi->eraseFromParent();

    for (auto *user : newPhi->users()) {
      if (auto *phiUser = ::cast_or_null<PhiInst>(user)) {
        if (inQueue.insert(phiUser).second) {
          queue.push_back(phiUser);
        }
      }
    }
  }

  for (Block &block : func) {
    for (auto it = block.begin(); it != block.end(); ) {
      if (auto *phi = ::cast_or_null<PhiInst>(&*it++)) {
        // Remove redundant PHIs.
        llvm::SmallPtrSet<PhiInst *, 10> phiCycle;

        std::function<bool(PhiInst *)> isDeadCycle = [&] (PhiInst *phi)  -> bool
        {
          if (!phiCycle.insert(phi).second) {
            return true;
          }

          for (User *user : phi->users()) {
            if (auto *nextPhi = ::cast_or_null<PhiInst>(user)) {
              if (!isDeadCycle(nextPhi)) {
                return false;
              }
              continue;
            }
            return false;
          }
          return true;
        };

        if (isDeadCycle(phi)) {
          for (PhiInst *deadPhi : phiCycle) {
            if (deadPhi == &*it) {
              ++it;
            }
            deadPhi->replaceAllUsesWith(nullptr);
            deadPhi->eraseFromParent();
          }
        }
      } else {
        break;
      }
    }
  }
  return llvm::Error::success();
}
