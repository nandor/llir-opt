// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <map>
#include <unordered_set>

#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/inst_visitor.h"
#include "core/inst_compare.h"
#include "core/analysis/dominator.h"
#include "core/analysis/call_graph.h"
#include "core/analysis/reference_graph.h"
#include "passes/store_to_load.h"



// -----------------------------------------------------------------------------
const char *StoreToLoadPass::kPassID = "store-to-load";

// -----------------------------------------------------------------------------
const char *StoreToLoadPass::GetPassName() const
{
  return "Store-To-Load Propagation";
}

// -----------------------------------------------------------------------------
static std::optional<std::pair<Atom *, unsigned>>
ToGlobal(Ref<Inst> addr)
{
  if (auto inst = ::cast_or_null<MovInst>(addr)) {
    if (auto e = ::cast_or_null<SymbolOffsetExpr>(inst->GetArg())) {
      if (auto atom = ::cast_or_null<Atom>(e->GetSymbol())) {
        if (atom->getParent()->size() == 1) {
          return std::make_pair(&*atom, e->GetOffset());
        }
      }
    }
    if (auto atom = ::cast_or_null<Atom>(inst->GetArg())) {
      if (atom->getParent()->size() == 1) {
        return std::make_pair(&*atom, 0);
      }
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
using StoreMap = std::map<Atom *, std::map<unsigned, StoreInst *>>;
using BlockToStores = std::map<Block *, StoreMap>;

// -----------------------------------------------------------------------------
class StoreToLoad final {
public:
  StoreToLoad(Prog &prog)
    : cg_(prog)
    , rg_(prog, cg_)
  {
    for (Data &data : prog.data()) {
      for (Object &object : data) {
        bool escapes = false;
        std::queue<Inst *> q;
        for (Atom &atom : object) {
          for (User *user : atom.users()) {
            if (auto *inst = ::cast_or_null<MovInst>(user)) {
              q.push(inst);
              continue;
            }
            if (auto *expr = ::cast_or_null<SymbolOffsetExpr>(user)) {
              for (User *exprUser : expr->users()) {
                if (auto *inst = ::cast_or_null<MovInst>(exprUser)) {
                  q.push(inst);
                  continue;
                }
                escapes = true;
                break;
              }
              if (escapes) {
                break;
              }
              continue;
            }
            escapes = true;
          }
        }
        while (!q.empty()) {
          auto *inst = q.front();
          q.pop();
          if (auto *mov = ::cast_or_null<MovInst>(inst)) {
            for (User *movUser : mov->users()) {
              q.push(::cast_or_null<Inst>(movUser));
            }
            continue;
          }
          if (::cast_or_null<MemoryLoadInst>(inst)) {
            continue;
          }
          if (::cast_or_null<MemoryStoreInst>(inst)) {
            continue;
          }
          escapes = true;
          break;
        }
        if (escapes) {
          for (Atom &atom : object) {
            escapes_.insert(&atom);
          }
        }
      }
    }
  }

  bool Run(Func &func);

  ReferenceGraph &GetReferenceGraph() { return rg_; }

  bool Escapes(Atom *atom) { return escapes_.count(atom); }

private:
  /// Call graph.
  CallGraph cg_;
  /// Set of referenced globals for each symbol.
  ReferenceGraph rg_;
  /// Set of objects whose pointers escape.
  std::unordered_set<Atom *> escapes_;
};

// -----------------------------------------------------------------------------
class StoreToLoadVisitor : public InstVisitor<void> {
public:
  StoreToLoadVisitor(StoreToLoad &stl, StoreMap &stores)
    : stores_(stores)
    , stl_(stl)
  {
  }

  void VisitInst(Inst &i) override { }

  void VisitStoreInst(StoreInst &store) override
  {
    auto size = GetSize(store.GetValue().GetType());
    if (auto g = ToGlobal(store.GetAddr())) {
      auto it = stores_.find(g->first);
      if (it != stores_.end()) {
        for (auto st = it->second.begin(); st != it->second.end(); ) {
          if (g->second <= st->first && st->first < g->second + size) {
            it->second.erase(st++);
          } else {
            ++st;
          }
        }
      }
      stores_[g->first][g->second] = &store;
    } else {
      stores_.clear();
    }
  }

  void VisitBarrierInst(BarrierInst &i) override
  {
    stores_.clear();
  }

  void VisitMemoryStoreInst(MemoryStoreInst &i) override
  {
    stores_.clear();
  }

  void VisitMemoryExchangeInst(MemoryExchangeInst &i) override
  {
    stores_.clear();
  }

  void VisitX86_FPUControlInst(X86_FPUControlInst &i) override
  {
    stores_.clear();
  }

  void VisitCallSite(CallSite &call) override
  {
    if (auto *f = call.GetDirectCallee()) {
      auto &n = stl_.GetReferenceGraph()[*f];
      if (n.HasIndirectCalls || n.HasRaise || n.HasBarrier) {
        stores_.clear();
      } else {
        for (auto it = stores_.begin(); it != stores_.end(); ) {
          auto *g = it->first;
          auto *o = g->getParent();
          if (stl_.Escapes(g) || n.Escapes.count(g) || n.Written.count(o)) {
            stores_.erase(it++);
          } else {
            ++it;
          }
        }
      }
    } else {
      stores_.clear();
    }
  }

private:
  /// Mapping to alter.
  StoreMap &stores_;
  /// Reference to the main context.
  StoreToLoad &stl_;
};

// -----------------------------------------------------------------------------
bool StoreToLoad::Run(Func &func)
{
  BlockToStores storesIn;
  BlockToStores storesOut;

  std::queue<Block *> q;
  q.push(&func.getEntryBlock());
  while (!q.empty()) {
    Block &block = *q.front();
    q.pop();

    bool changed = false;
    StoreMap blockStoresIn;
    if (!block.pred_empty()) {
      auto begin = block.pred_begin();
      blockStoresIn = storesOut[*begin];
      for (auto it = std::next(begin); it != block.pred_end(); ++it) {
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
    for (Inst &inst : block) {
      StoreToLoadVisitor(*this, blockStoresOut).Dispatch(inst);
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
      for (auto *succ : block.successors()) {
        q.push(succ);
      }
    }
  }

  for (auto &block : func) {
    auto stores = storesIn[&block];
    for (auto it = block.begin(); it != block.end(); ) {
      Inst &inst = *it++;
      if (auto *load = ::cast_or_null<LoadInst>(&inst)) {
        if (auto g = ToGlobal(load->GetAddr())) {
          auto it = stores.find(g->first);
          if (it == stores.end()) {
            continue;
          }
          auto ot = it->second.find(g->second);
          if (ot == it->second.end()) {
            continue;
          }
          auto value = ot->second->GetValue();
          if (load->GetType() != value.GetType()) {
            continue;
          }
          load->replaceAllUsesWith(value);
          load->eraseFromParent();
        }
      } else {
        StoreToLoadVisitor(*this, stores).Dispatch(inst);
      }
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
bool StoreToLoadPass::Run(Prog &prog)
{
  StoreToLoad stl(prog);
  bool changed = false;
  for (Func &func : prog) {
    changed = stl.Run(func) || changed;
  }
  return changed;
}
