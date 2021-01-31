// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Statistic.h>
#include <llvm/Support/Debug.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/pass_manager.h"
#include "passes/caml_global_simplify.h"

#define DEBUG_TYPE "caml-global-simplify"

STATISTIC(NumReferencesRemoved, "Number of references removed");



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
  return Visit(globals->getParent());
}

// -----------------------------------------------------------------------------
bool CamlGlobalSimplifyPass::Visit(Object *object)
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
bool CamlGlobalSimplifyPass::Visit(Atom *atom)
{
  auto *obj = atom->getParent();
  if (!atom->IsLocal()) {
    return false;
  }
  if (obj->size() != 1) {
    return false;
  }
  if (atom->use_size() != 1) {
    return false;
  }
  return Visit(obj);
}
