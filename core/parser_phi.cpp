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
void Parser::EndFunction()
{
  // Check if function is ill-defined.
  if (func_->empty()) {
    l_.Error(func_, "Empty function");
  } else if (!func_->rbegin()->GetTerminator()) {
    l_.Error(func_, "Function not terminated");
  }

  PhiPlacement();

  func_ = nullptr;
  block_ = nullptr;

  vregs_.clear();
}

// -----------------------------------------------------------------------------
void Parser::PhiPlacement()
{
  // Construct the dominator tree & find dominance frontiers.
  DominatorTree DT(*func_);
  DominanceFrontier DF;
  DF.analyze(DT);

  // Find all definitions of all variables.
  llvm::DenseSet<unsigned> custom;
  for (Block &block : *func_) {
    for (PhiInst &inst : block.phis()) {
      for (Use &use : inst.operands()) {
        const auto vreg = reinterpret_cast<uint64_t>(use.get());
        if (vreg & 1) {
          custom.insert(vreg >> 1);
        }
      }
    }
  }

  llvm::DenseMap<unsigned, std::queue<Inst *>> sites;
  for (Block &block : *func_) {
    llvm::DenseMap<unsigned, Inst *> localSites;
    for (Inst &inst : block) {
      if (auto it = vregs_.find(&inst); it != vregs_.end()) {
        unsigned vreg = it->second;
        if (inst.GetNumRets() > 0 && custom.count(vreg) == 0) {
          localSites[vreg] = &inst;
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
      auto *inst = q.front();
      q.pop();
      auto *block = inst->getParent();
      if (auto *node = DT.getNode(block)) {
        for (auto &front : DF.calculate(DT, node)) {
          bool found = false;
          for (PhiInst &phi : front->phis()) {
            if (auto it = vregs_.find(&phi); it != vregs_.end()) {
              if (it->second == var.first) {
                found = true;
                break;
              }
            }
          }

          // If the PHI node was not added already, add it.
          if (!found) {
            auto *phi = new PhiInst(inst->GetType(0), {});
            front->AddPhi(phi);
            vregs_[phi] = var.first;
            q.push(phi);
          }
        }
      }
    }
  }

  // Renaming variables to point to definitions or PHI nodes.
  llvm::DenseMap<unsigned, std::stack<Inst *>> vars;
  llvm::SmallPtrSet<Block *, 8> blocks;
  std::function<void(Block *block)> rename = [&](Block *block) {
    // Add the block to the set of visited ones.
    blocks.insert(block);

    // Register the names of incoming PHIs.
    for (PhiInst &phi : block->phis()) {
      auto it = vregs_.find(&phi);
      if (it != vregs_.end()) {
        vars[it->second].push(&phi);
      }
    }

    // Rename all non-phis, registering them in the map.
    for (Inst &inst : *block) {
      if (inst.Is(Inst::Kind::PHI)) {
        continue;
      }

      for (Use &use : inst.operands()) {
        const auto vreg = reinterpret_cast<uint64_t>(use.get());
        if (vreg & 1) {
          auto &stk = vars[vreg >> 1];
          if (stk.empty()) {
            l_.Error(
                func_,
                block,
                "undefined vreg: " + std::to_string(vreg >> 1)
            );
          }
          use = stk.top();
        }
      }

      if (auto it = vregs_.find(&inst); it != vregs_.end()) {
        vars[it->second].push(&inst);
      }
    }

    // Handle PHI nodes in successors.
    for (Block *succ : block->successors()) {
      for (PhiInst &phi : succ->phis()) {
        if (phi.HasValue(block)) {
          auto *value = phi.GetValue(block);
          const auto vreg = reinterpret_cast<uint64_t>(value);
          if (vreg & 1) {
            phi.Add(block, vars[vreg >> 1].top());
          }
        } else {
          auto &stk = vars[vregs_[&phi]];
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
      rename(child->getBlock());
    }

    // Pop definitions of this block from the stack.
    for (auto it = block->rbegin(); it != block->rend(); ++it) {
      if (auto jt = vregs_.find(&*it); jt != vregs_.end()) {
        auto &q = vars[jt->second];
        assert(q.top() == &*it && "invalid type");
        q.pop();
      }
    }
  };
  rename(DT.getRoot());

  // Remove blocks which are trivially dead.
  std::vector<PhiInst *> queue;
  std::set<PhiInst *> inQueue;
  for (auto it = func_->begin(); it != func_->end(); ) {
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
      if (auto *inst = ::dyn_cast_or_null<Inst>(phi->GetValue(i))) {
        isValue = isValue || inst->GetType(0) == Type::V64;
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
      if (auto *phiUser = ::dyn_cast_or_null<PhiInst>(user)) {
        if (inQueue.insert(phiUser).second) {
          queue.push_back(phiUser);
        }
      }
    }
  }

  for (Block &block : *func_) {
    for (auto it = block.begin(); it != block.end(); ) {
      if (auto *phi = ::dyn_cast_or_null<PhiInst>(&*it++)) {
        // Remove redundant PHIs.
        llvm::SmallPtrSet<PhiInst *, 10> phiCycle;

        std::function<bool(PhiInst *)> isDeadCycle = [&] (PhiInst *phi)  -> bool
        {
          if (!phiCycle.insert(phi).second) {
            return true;
          }

          for (User *user : phi->users()) {
            if (auto *nextPhi = ::dyn_cast_or_null<PhiInst>(user)) {
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
}
