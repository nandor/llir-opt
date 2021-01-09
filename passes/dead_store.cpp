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
  bool changed = RemoveTautologicalStores(prog);
  for (Func &func : prog) {
    changed = RemoveLocalDeadStores(func) || changed;
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
bool DeadStorePass::RemoveLocalDeadStores(Func &func)
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

// -----------------------------------------------------------------------------
bool DeadStorePass::RemoveTautologicalStores(Prog &prog)
{
  bool changed = false;

  for (Data &data : prog.data()) {
    for (Object &object : data) {
      if (object.size() != 1) {
        continue;
      }

      Atom &atom = *object.begin();
      if (!atom.IsLocal() || atom.empty()) {
        continue;
      }

      std::set<StoreInst *> stores;
      bool doesNotEscape = true;
      std::queue<std::pair<Inst *, Inst *>> q;
      for (auto *user : atom.users()) {
        auto *mov = ::cast_or_null<MovInst>(user);
        if (!mov) {
          doesNotEscape = false;
          break;
        }
        for (auto *movUser : mov->users()) {
          q.emplace(::cast<Inst>(movUser), nullptr);
        }
      }

      std::set<Inst *> v;
      while (doesNotEscape && !q.empty()) {
        auto [inst, from] = q.front();
        q.pop();
        if (!v.insert(inst).second) {
          continue;
        }
        switch (inst->GetKind()) {
          case Inst::Kind::LOAD: {
            continue;
          }
          case Inst::Kind::STORE: {
            auto *store = static_cast<StoreInst *>(inst);
            if (store->GetValue().Get() == from) {
              doesNotEscape = false;
              continue;
            }
            if (!store->GetValue()->IsConstant()) {
              doesNotEscape = false;
              continue;
            }
            stores.insert(store);
            continue;
          }
          case Inst::Kind::MOV:
          case Inst::Kind::ADD:
          case Inst::Kind::SUB:
          case Inst::Kind::PHI: {
            for (User *user : inst->users()) {
              if (auto *userInst = ::cast_or_null<Inst>(user)) {
                q.emplace(userInst, inst);
              }
            }
            continue;
          }
          default: {
            doesNotEscape = false;
            continue;
          }
        }
        if (!doesNotEscape) {
          break;
        }
      }
      if (!doesNotEscape || stores.empty()) {
        continue;
      }

      bool isTautological = true;
      for (auto *store : stores) {
        auto mov = ::cast_or_null<MovInst>(store->GetValue());
        if (!mov) {
          isTautological = false;
          break;
        }

        auto constInt = ::cast_or_null<ConstantInt>(mov->GetArg());
        if (!constInt || mov.GetType() != Type::I64) {
          isTautological = false;
          break;
        }
        const auto &v = constInt->GetValue();

        Item &item = *atom.begin();
        switch (item.GetKind()) {
          case Item::Kind::INT8:
          case Item::Kind::INT16:
          case Item::Kind::INT32:
          case Item::Kind::STRING:
          case Item::Kind::EXPR:
          case Item::Kind::FLOAT64: {
            isTautological = false;
            break;
          }
          case Item::Kind::INT64: {
            if (item.GetInt64() != v.getSExtValue()) {
              isTautological = false;
              break;
            }
          }
          case Item::Kind::SPACE: {
            if (v.getSExtValue() != 0) {
              isTautological = false;
              break;
            }
          }
        }
        if (!isTautological) {
          break;
        }
      }
      if (!isTautological) {
        continue;
      }
      for (auto *store : stores) {
        store->eraseFromParent();
      }
    }
  }

  return changed;
}
