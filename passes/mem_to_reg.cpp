// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>
#include <queue>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/analysis/dominator.h"
#include "passes/mem_to_reg.h"



// -----------------------------------------------------------------------------
const char *MemoryToRegisterPass::kPassID = "mem-to-reg";

// -----------------------------------------------------------------------------
using PtrUses = std::map<Inst *, std::pair<int64_t, Type>>;

// -----------------------------------------------------------------------------
std::optional<PtrUses> FindUses(
    Inst *inst,
    int64_t off,
    llvm::MaybeAlign align)
{
  PtrUses uses;

  std::queue<std::tuple<Inst *, Inst *, int64_t>> q;
  for (auto *user : inst->users()) {
    q.emplace(::cast<Inst>(user), inst, off);
  }

  while (!q.empty()) {
    auto [inst, arg, off] = q.front();
    q.pop();
    switch (auto k = inst->GetKind()) {
      case Inst::Kind::LOAD: {
        auto &load = static_cast<const LoadInst &>(*inst);
        uses[inst] = std::make_pair(off, load.GetType());
        break;
      }
      case Inst::Kind::STORE: {
        auto &store = static_cast<const StoreInst &>(*inst);
        if (&*store.GetValue() == arg) {
          return {};
        }
        uses[inst] = std::make_pair(off, store.GetValue().GetType());
        break;
      }
      case Inst::Kind::MOV: {
        for (auto *user : inst->users()) {
          q.emplace(::cast<Inst>(user), inst, off);
        }
        break;
      }
      case Inst::Kind::ADD:
      case Inst::Kind::OR:
      case Inst::Kind::AND: {
        auto &op = static_cast<const BinaryInst &>(*inst);
        ConstRef<MovInst> mov;
        if (&*op.GetRHS() == arg) {
          mov = ::cast_or_null<const MovInst>(op.GetLHS());
        } else {
          mov = ::cast_or_null<const MovInst>(op.GetRHS());
        }
        if (!mov) {
          return {};
        }
        auto arg = ::cast_or_null<ConstantInt>(mov->GetArg());
        if (!arg) {
          return {};
        }

        int64_t newOffset = 0;
        switch (inst->GetKind()) {
          default: llvm_unreachable("invalid instruction");
          case Inst::Kind::ADD: {
            newOffset = off + arg->GetInt();
            break;
          }
          case Inst::Kind::OR: {
            if (!align) {
              return {};
            }
            if (arg->GetInt() >= align.valueOrOne().value()) {
              return {};
            }
            newOffset = off | arg->GetInt();
            break;
          }
          case Inst::Kind::AND: {
            llvm_unreachable("not implemented");
          }
        }
        for (auto *user : inst->users()) {
          q.emplace(::cast<Inst>(user), inst, newOffset);
        }
        continue;
      }
      case Inst::Kind::SUB: {
        auto &op = static_cast<const BinaryInst &>(*inst);
        if (&*op.GetRHS() == arg) {
          return {};
        }
        ConstRef<MovInst> mov = ::cast_or_null<const MovInst>(op.GetRHS());
        if (!mov) {
          return {};
        }
        auto arg = ::cast_or_null<ConstantInt>(mov->GetArg());
        if (!arg) {
          return {};
        }
        int64_t newOffset = off - arg->GetInt();
        for (auto *user : inst->users()) {
          q.emplace(::cast<Inst>(user), inst, newOffset);
        }
        break;
      }
      default: {
        return {};
      }
    }
  }
  return uses;
}

// -----------------------------------------------------------------------------
bool MemoryToRegisterPass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    if (!func.HasAddressTaken()) {
      changed = Run(func) || changed;
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
static void ReplaceObject(
    Func &func,
    const PtrUses &uses,
    const llvm::DenseMap<int64_t, Type> &offsets)
{
  // Build the dominator tree for the function.
  DominatorTree DT(func);
  DominanceFrontier DF;
  DF.analyze(DT);

  // Find the stores and place PHIs at their frontiers.
  for (auto [off, ty] : offsets) {
    std::queue<Block *> q;
    for (auto [inst, use] : uses) {
      if (use.first != off) {
        continue;
      }
      assert(use.second == ty && "invalid type");
      if (auto *store = ::cast_or_null<const StoreInst>(inst)) {
        q.push(store->getParent());
      }
    }

    std::unordered_map<Block *, PhiInst *> phis;
    while (!q.empty()) {
      Block *block = q.front();
      q.pop();
      if (auto *node = DT.getNode(block)) {
        for (Block *front : DF.calculate(DT, node)) {
          if (auto it = phis.find(front); it == phis.end()) {
            auto *phi = new PhiInst(ty, {});
            front->AddPhi(phi);
            phis[front] = phi;
            q.push(front);
          }
        }
      }
    }

    std::stack<Ref<Inst>> insts;
    std::function<void(Block *block)> rewrite =
      [&, ty = ty, off = off] (Block *block) {
        // Rewrite the loads and stores.
        Ref<Inst> definition = nullptr;
        for (auto it = block->begin(); it != block->end(); ) {
          Inst *inst = &*it++;
          auto ut = uses.find(inst);
          if (ut == uses.end() || ut->second.first != off) {
            continue;
          }
          if (auto *load = ::cast_or_null<LoadInst>(inst)) {
            if (definition) {
              load->replaceAllUsesWith(definition);
            } else {
              if (insts.empty()) {
                auto *undef = new UndefInst(ty, load->GetAnnots());
                block->AddInst(undef, load);
                load->replaceAllUsesWith(undef);
              } else {
                load->replaceAllUsesWith(insts.top());
              }
            }
            continue;
          }
          if (auto *store = ::cast_or_null<StoreInst>(inst)) {
            definition = store->GetValue();
            inst->eraseFromParent();
            continue;
          }
          llvm_unreachable("invalid frame access");
        }
        if (definition) {
          insts.push(definition);
        }

        // Rewrites PHIs in successors.
        UndefInst *undef = nullptr;
        for (Block *succ : block->successors()) {
          auto it = phis.find(succ);
          if (it == phis.end()) {
            continue;
          }
          PhiInst *phi = it->second;
          assert(!phi->HasValue(block) && "phi should not have been placed");
          if (definition) {
            phi->Add(block, definition);
          } else {
            if (!undef) {
              undef = new UndefInst(ty, {});
              block->AddInst(undef, block->GetTerminator());
            }
            phi->Add(block, undef);
          }
        }
        // Rewrite child nodes in dominator tree.
        for (const auto *child : *DT[block]) {
          rewrite(child->getBlock());
        }
        // Pop definitions from this block.
        if (definition) {
          insts.pop();
        }
      };

    rewrite(DT.getRoot());
    assert(insts.empty() && "invalid rewrite");
  }
}

// -----------------------------------------------------------------------------
bool MemoryToRegisterPass::Run(Func &func)
{
  const auto &objects = func.objects();
  bool changed = false;
  for (unsigned i = 0, n = objects.size(); i < n; ++i) {
    auto &obj = objects[i];

    PtrUses allUses;
    bool escapes = false;
    for (Block &block : func) {
      for (Inst &inst : block) {
        auto *frame = ::cast_or_null<FrameInst>(&inst);
        if (!frame || frame->GetObject() != obj.Index) {
          continue;
        }
        auto uses = FindUses(frame, frame->GetOffset(), obj.Alignment);
        if (!uses) {
          escapes = true;
        } else  {
          for (auto [inst, use] : *uses) {
            allUses[inst] = use;
          }
        }
      }
    }

    if (escapes || allUses.empty()) {
      continue;
    }
    // Try to infer a type for the fields looking at stores and loads.
    bool overlap = false;
    llvm::DenseMap<int64_t, Type> offsets;
    for (auto [inst, use] : allUses) {
      if (auto it = offsets.find(use.first); it != offsets.end()) {
        if (it->second != use.second) {
          overlap = true;
          break;
        }
      } else {
        offsets[use.first] = use.second;
      }
    }
    for (auto [offset, type] : offsets) {
      if (offset < 0) {
        overlap = true;
        break;
      }
      for (unsigned i = 1; i < GetSize(type); ++i) {
        if (offsets.find(offset + i) != offsets.end()) {
          overlap = true;
          break;
        }
      }
    }
    if (overlap) {
      continue;
    }
    // If there is no overlap, structure can be broken down.
    ReplaceObject(func, allUses, offsets);
    changed = true;
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *MemoryToRegisterPass::GetPassName() const
{
  return "Structure to Register";
}
