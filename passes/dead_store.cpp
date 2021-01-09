// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <map>

#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/inst_visitor.h"
#include "core/inst_compare.h"
#include "core/analysis/dominator.h"
#include "passes/dead_store.h"




// -----------------------------------------------------------------------------
const char *DeadStorePass::kPassID = "dead-store";


// -----------------------------------------------------------------------------
const char *DeadStorePass::GetPassName() const
{
  return "Trivial Dead Store Elimination";
}

// -----------------------------------------------------------------------------
bool DeadStorePass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    changed = Run(func) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
using StoreMap = std::map<Global *, MemoryStoreInst *>;
using BlockToStores = std::map<Block *, StoreMap>;

// -----------------------------------------------------------------------------
static Global *ToGlobal(Ref<Inst> addr)
{
  if (auto inst = ::cast_or_null<MovInst>(addr)) {
    if (auto e = ::cast_or_null<SymbolOffsetExpr>(inst->GetArg())) {
      if (auto atom = ::cast_or_null<Atom>(e->GetSymbol())) {
        if (atom->getParent()->size() == 1 && e->GetOffset() == 0) {
          return &*atom;
        }
      }
    }
    if (auto atom = ::cast_or_null<Atom>(inst->GetArg())) {
      if (atom->getParent()->size() == 1) {
        return &*atom;
      }
    }
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
class DeadStoreVisitor : public InstVisitor<void> {
public:
  DeadStoreVisitor(StoreMap &stores) : stores_(stores) {}

  void VisitInst(Inst &i) { }

  void VisitMemoryStoreInst(MemoryStoreInst &store)
  {
    if (auto *g = ToGlobal(store.GetAddr())) {
      stores_[g] = &store;
    } else {
      stores_.clear();
    }
  }

  void VisitMemoryLoadInst(MemoryLoadInst &store) { stores_.clear(); }
  void VisitBarrierInst(BarrierInst &i) { stores_.clear(); }
  void VisitMemoryExchangeInst(MemoryExchangeInst &i) { stores_.clear(); }
  void VisitCallSite(CallSite &i) { stores_.clear(); }
  void VisitX86_FPUControlInst(X86_FPUControlInst &i) { stores_.clear(); }

private:
  /// Mapping to alter.
  StoreMap &stores_;
};



// -----------------------------------------------------------------------------
bool DeadStorePass::Run(Func &func)
{
  BlockToStores storesIn;
  BlockToStores storesOut;

  std::queue<Block *> q;
  for (Block &block : func) {
    if (block.succ_empty()) {
      q.push(&block);
    }
  }

  while (!q.empty()) {
    Block &block = *q.front();
    q.pop();

    bool changed = false;
    StoreMap blockStoresIn;
    if (!block.succ_empty()) {
      auto begin = block.succ_begin();
      blockStoresIn = storesOut[*begin];
      for (auto it = std::next(begin); it != block.succ_end(); ++it) {
        auto &outs = storesOut[*it];
        for (auto bt = blockStoresIn.begin(); bt != blockStoresIn.end(); ) {
          auto et = outs.find(bt->first);
          if (et == outs.end() || et->second != bt->second) {
            blockStoresIn.erase(bt++);
            continue;
          } else {
            ++bt;
          }
        }
      }
    }

    {
      auto it = storesIn.emplace(&block, blockStoresIn);
      if (it.second) {
        changed = true;
      } else if (it.first->second != blockStoresIn) {
        it.first->second = blockStoresIn;
        changed = true;
      }
    }

    StoreMap blockStoresOut(blockStoresIn);
    for (auto it = block.rbegin(); it != block.rend(); ++it) {
      DeadStoreVisitor(blockStoresOut).Dispatch(*it);
    }

    {
      auto it = storesOut.emplace(&block, blockStoresOut);
      if (it.second) {
        changed = true;
      } else if (it.first->second != blockStoresOut) {
        it.first->second = blockStoresOut;
        changed = true;
      }
    }

    if (changed) {
      for (auto *succ : block.predecessors()) {
        q.push(succ);
      }
    }
  }

  for (auto &block : func) {
    auto stores = storesIn[&block];
    for (auto it = block.rbegin(); it != block.rend(); ) {
      Inst &inst = *it++;
      if (auto *store = ::cast_or_null<MemoryStoreInst>(&inst)) {
        if (auto *g = ToGlobal(store->GetAddr())) {
          if (auto it = stores.find(g); it != stores.end()) {
            store->eraseFromParent();
            continue;
          }
        }
      }
      DeadStoreVisitor(stores).Dispatch(inst);
    }
  }

  return false;
}
