// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/pass_manager.h"
#include "passes/caml_global_simplify.h"



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
  for (Atom &atom : *object) {
    for (auto it = atom.begin(); it != atom.end(); ) {
      Item &item = *it++;
      auto *expr = ::cast_or_null<SymbolOffsetExpr>(item.AsExpr());
      if (!expr || expr->use_size() != 1) {
        continue;
      }
      auto *sym = expr->GetSymbol();
      if (sym->use_size() != 1) {
        continue;
      }
      switch (sym->GetKind()) {
        case Global::Kind::ATOM: {
          auto *atom = static_cast<Atom *>(sym);
          if (atom->use_size() != 1 || !atom->IsLocal()) {
            continue;
          }
          auto *obj = atom->getParent();
          if (obj->size() != 1) {
            continue;
          }
          changed = Visit(obj) || changed;
          continue;
        }
        case Global::Kind::FUNC: {
          atom.AddItem(new Item(static_cast<int64_t>(0)), &item);
          item.eraseFromParent();
          changed = true;
          continue;
        }
        case Global::Kind::EXTERN:
        case Global::Kind::BLOCK: {
          continue;
        }
      }
      llvm_unreachable("invalid global kind");
    }
  }
  return changed;
}
