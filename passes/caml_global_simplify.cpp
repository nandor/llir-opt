// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>

#include <llvm/ADT/Statistic.h>
#include <llvm/Support/Debug.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/analysis/init_path.h"
#include "passes/caml_global_simplify.h"

#define DEBUG_TYPE "caml-global-simplify"

STATISTIC(NumReferencesRemoved, "References removed");
STATISTIC(NumStoresRemoved, "Stores removed");
STATISTIC(NumLoadsFolded, "Loads folded");



// -----------------------------------------------------------------------------
class CamlGlobalSimplifier final {
public:
  /// Set up the transformation.
  CamlGlobalSimplifier(Prog &prog, Func *entry) : path_(prog, entry) {}

  /// Recursively simplify objects starting at caml_globals.
  bool Visit(Object *object);
  /// Simplify an atom.
  bool Visit(Atom *atom);

private:
  using StoreMap = std::map<int64_t, std::set<MemoryStoreInst *>>;
  using LoadMap = std::map<int64_t, std::set<MemoryLoadInst *>>;

  /// Simplify store-only object.
  bool SimplifyStoreOnly(Atom *atom, const StoreMap &stores);
  /// Simplify load-only object.
  bool SimplifyLoadOnly(Atom *atom, const LoadMap &loads);
  /// Simplify unused offsets.
  bool SimplifyUnused(Atom *atom, const StoreMap &stores, const LoadMap &loads);

private:
  /// Reference to the init path analysis.
  InitPath path_;
  /// Set of stores which are the unique store to a location.
  std::set<MemoryStoreInst *> unique_;
};


// -----------------------------------------------------------------------------
bool CamlGlobalSimplifier::Visit(Object *object)
{
  bool changed = false;
  auto *data = object->getParent();
  for (Atom &atom : *object) {
    for (auto it = atom.begin(); it != atom.end(); ) {
      Item &item = *it++;
      auto *expr = ::cast_or_null<SymbolOffsetExpr>(item.AsExpr());
      if (!expr) {
        continue;
      }

      auto *sym = expr->GetSymbol();
      if (auto *ref = ::cast_or_null<Atom>(sym)) {
        auto *refData = ref->getParent()->getParent();
        if (refData == data) {
          if (expr->use_size() == 1) {
            changed = Visit(ref) || changed;
          }
          continue;
        }
      }

      LLVM_DEBUG(llvm::dbgs()
          << "Removed " << sym->getName()
          << " from " << atom.getName()
          << "\n"
      );
      NumReferencesRemoved++;
      atom.AddItem(new Item(static_cast<int64_t>(0)), &item);
      item.eraseFromParent();
      changed = true;
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool CamlGlobalSimplifier::Visit(Atom *atom)
{
  auto *object = atom->getParent();
  if (!atom->IsLocal()) {
    return false;
  }
  if (object->size() != 1) {
    return false;
  }

  if (atom->use_size() != 1) {
    std::set<std::pair<MovInst *, int64_t>> instUsers;
    unsigned dataUses = 0;
    for (User *user : atom->users()) {
      if (auto *inst = ::cast_or_null<MovInst>(user)) {
        instUsers.emplace(inst, 0);
        continue;
      }
      if (auto *expr = ::cast_or_null<SymbolOffsetExpr>(user)) {
        for (auto *exprUser : expr->users()) {
          if (auto *inst = ::cast_or_null<MovInst>(exprUser)) {
            instUsers.emplace(inst, expr->GetOffset());
          } else {
            dataUses++;
          }
        }
        continue;
      }
      dataUses++;
    }
    if (dataUses > 1) {
      return false;
    }

    StoreMap stores;
    LoadMap loads;
    for (auto [inst, offset] : instUsers) {
      for (User *user : inst->users()) {
        if (auto *store = ::cast_or_null<MemoryStoreInst>(user)) {
          if (store->GetValue() == inst->GetSubValue(0)) {
            return false;
          }
          stores[offset].insert(store);
          continue;
        }
        if (auto *load = ::cast_or_null<MemoryLoadInst>(user)) {
          loads[offset].insert(load);
          continue;
        }
        return false;
      }
    }

    if (loads.empty()) {
      return SimplifyStoreOnly(atom, stores);
    } else if (stores.empty()) {
      return SimplifyLoadOnly(atom, loads);
    } else {
      return SimplifyUnused(atom, stores, loads);
    }
  }
  return Visit(object);
}

// -----------------------------------------------------------------------------
bool CamlGlobalSimplifier::SimplifyStoreOnly(Atom *atom, const StoreMap &stores)
{
  // Object does not alias and is never loaded. Erase all stores.
  LLVM_DEBUG(llvm::dbgs() << atom->getName() << " store-only\n");
  bool changed = false;
  for (auto &[off, insts] : stores) {
    for (auto *store : insts) {
      NumStoresRemoved++;
      store->eraseFromParent();
      changed = true;
    }
  }
  // Static pointees could be eliminated.
  return Visit(atom->getParent()) || changed;
}

// -----------------------------------------------------------------------------
bool CamlGlobalSimplifier::SimplifyLoadOnly(Atom *atom, const LoadMap &loads)
{
  // Object is never written, so all stores to it can be eliminated.
  LLVM_DEBUG(llvm::dbgs() << atom->getName() << " load-only\n");
  bool changed = false;
  for (auto &[off, insts] : loads) {
    for (auto *inst : insts) {
      auto ty = inst->GetType();
      if (auto *v = atom->getParent()->Load(off, ty)) {
        auto *mov = new MovInst(ty, v, inst->GetAnnots());
        inst->getParent()->AddInst(mov, inst);
        inst->replaceAllUsesWith(mov);
        inst->eraseFromParent();
        NumLoadsFolded++;
        changed = true;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool CamlGlobalSimplifier::SimplifyUnused(
    Atom *atom,
    const StoreMap &stores,
    const LoadMap &loads)
{
  bool changed = false;
  std::set<std::pair<int64_t, int64_t>> offsets, stored, loaded;
  // Find the offsets where values are stored to and loaded from.
  // Bail out if there is any overlap or size mismatch between fields.
  for (auto &[start, insts] : stores) {
    for (auto *store : insts) {
      auto end = start + GetSize(store->GetValue().GetType());
      offsets.emplace(start, end);
      stored.emplace(start, end);
      for (auto [offStart, offEnd] : offsets) {
        if (end <= offStart || offEnd <= start) {
          continue;
        }
        if (start == offStart && end == offEnd) {
          continue;
        }
        return false;
      }
    }
  }
  for (auto &[start, insts] : loads) {
    for (auto *load : insts) {
      auto end = start + GetSize(load->GetType());
      offsets.emplace(start, end);
      loaded.emplace(start, end);
      for (auto [offStart, offEnd] : offsets) {
        if (end <= offStart || offEnd <= start) {
          continue;
        }
        if (start == offStart && end == offEnd) {
          continue;
        }
        return false;
      }
    }
  }
  // Remove stores which are never read.
  for (const auto &loc : stored) {
    if (loaded.count(loc)) {
      continue;
    }
    LLVM_DEBUG(llvm::dbgs()
        << atom->getName() << "+" << loc.first << "," << loc.second
        << " store-only\n"
    );
    for (auto [off, insts] : stores) {
      if (off == loc.first) {
        for (auto *store : insts) {
          NumStoresRemoved++;
          store->eraseFromParent();
          changed = true;
        }
      }
    }
  }
  // Evaluate loads which are never written.
  for (const auto &loc : loaded) {
    if (stored.count(loc)) {
      continue;
    }
    LLVM_DEBUG(llvm::dbgs()
        << atom->getName() << "+" << loc.first << "," << loc.second
        << " load-only\n"
    );
    llvm_unreachable("not implemented");
  }
  // Recurse and fold unused fields.
  int64_t offset = 0;
  auto *data = atom->getParent()->getParent();
  for (auto it = atom->begin(); it != atom->end(); ) {
    Item &item = *it++;
    int64_t start = offset;
    int64_t end = start + item.GetSize();
    offset = end;

    // Only simplify references.
    auto *expr = ::cast_or_null<SymbolOffsetExpr>(item.AsExpr());
    if (!expr) {
      continue;
    }

    // Skip the item if it overlaps with a load or a store.
    bool overlaps = false;
    for (auto [accStart, accEnd] : offsets) {
      if (end <= accStart || accEnd <= start) {
        continue;
      }
      overlaps = true;
      break;
    }
    if (overlaps) {
      continue;
    }

    // Simplify other references.
    auto *sym = expr->GetSymbol();
    if (auto *ref = ::cast_or_null<Atom>(sym)) {
      auto *refData = ref->getParent()->getParent();
      if (refData == data) {
        if (expr->use_size() == 1) {
          changed = Visit(ref) || changed;
        }
        continue;
      }
    }

    LLVM_DEBUG(llvm::dbgs()
        << "Removed " << sym->getName()
        << " from " << atom->getName()
        << "\n"
    );
    NumReferencesRemoved++;
    atom->AddItem(new Item(static_cast<int64_t>(0)), &item);
    item.eraseFromParent();
    changed = true;
  }
  // TODO: Record stores which are bypassed.
  return changed;
}


// -----------------------------------------------------------------------------
const char *CamlGlobalSimplifyPass::kPassID = "caml-global-simplify";

// -----------------------------------------------------------------------------
const char *CamlGlobalSimplifyPass::GetPassName() const
{
  return "OCaml Global Data Item Simplification";
}

// -----------------------------------------------------------------------------
bool CamlGlobalSimplifyPass::Run(Prog &prog)
{
  const auto &cfg = GetConfig();
  if (!cfg.Static) {
    return false;
  }
  auto *globals = ::cast_or_null<Atom>(prog.GetGlobal("caml_globals"));
  if (!globals) {
    return false;
  }

  const std::string start = cfg.Entry.empty() ? "_start" : cfg.Entry;
  CamlGlobalSimplifier simpl(prog, ::cast_or_null<Func>(prog.GetGlobal(start)));

  bool changed = false;
  changed = simpl.Visit(globals->getParent()) || changed;
  return changed;
}
