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
#include "passes/caml_global_simplify.h"

#define DEBUG_TYPE "caml-global-simplify"

STATISTIC(NumReferencesRemoved, "References removed");
STATISTIC(NumStoresRemoved, "Stores removed");
STATISTIC(NumLoadsFolded, "Loads folded");


// -----------------------------------------------------------------------------
class CamlGlobalSimplifier final {
public:
  /// Recursively simplify objects starting at caml_globals.
  bool Visit(Object *object);
  /// Simplify an atom.
  bool Visit(Atom *atom);
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

      LLVM_DEBUG(llvm::dbgs() << "Removed " << sym->getName() << "\n");
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

    std::map<int64_t, std::set<MemoryStoreInst *>> stores;
    std::map<int64_t, std::set<MemoryLoadInst *>> loads;
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
      return changed;
    }

    if (stores.empty()) {
      bool changed = false;
      LLVM_DEBUG(llvm::dbgs() << atom->getName() << " load-only\n");
      for (auto &[off, insts] : loads) {
        for (auto *inst : insts) {
          auto ty = inst->GetType();
          if (auto *v = object->Load(off, ty)) {
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
    return false;
  }
  return Visit(object);
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
  if (!GetConfig().Static) {
    return false;
  }
  auto *globals = ::cast_or_null<Atom>(prog.GetGlobal("caml_globals"));
  if (!globals) {
    return false;
  }

  CamlGlobalSimplifier simpl;

  bool changed = false;
  changed = simpl.Visit(globals->getParent()) || changed;
  return changed;
}
