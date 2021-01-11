// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>

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
static void Closure(
    std::set<Object *> &reachable,
    Object *object)
{
  if (!reachable.insert(object).second) {
    return;
  }
  for (Atom &atom : *object) {
    for (auto it = atom.begin(); it != atom.end(); ) {
      Item &item = *it++;
      auto *expr = ::cast_or_null<SymbolOffsetExpr>(item.AsExpr());
      if (!expr) {
        continue;
      }
      if (auto *sym = ::cast_or_null<Atom>(expr->GetSymbol())) {
        Closure(reachable, sym->getParent());
      }
    }
  }
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

  // Find the set of objects reachable from the start symbol.
  std::set<Object *> globalReach;
  Closure(globalReach, globals->getParent());

  // Find the set of objects reached from code.
  std::set<Object *> rootReach;
  for (Data &data : prog.data()) {
    for (Object &object : data) {
      if (&object == globals->getParent()) {
        continue;
      }
      bool referenced = false;
      for (Atom &atom : object) {
        if (!atom.IsLocal()) {
          referenced = true;
          continue;
        }
        for (User *user : atom.users()) {
          if (::cast_or_null<Inst>(user)) {
            referenced = true;
            break;
          }
        }
        if (referenced) {
          break;
        }
      }
      if (referenced) {
        Closure(rootReach, &object);
      }
    }
  }

  // Find caml objects which are only reachable from globals.
  bool changed = false;
  for (Object *object : globalReach) {
    if (rootReach.count(object)) {
      continue;
    }

    for (Atom &atom : *object) {
      for (auto it = atom.begin(); it != atom.end(); ) {
        Item &item = *it++;
        auto *expr = ::cast_or_null<SymbolOffsetExpr>(item.AsExpr());
        if (!expr) {
          continue;
        }
        auto *func = ::cast_or_null<Func>(expr->GetSymbol());
        if (!func) {
          continue;
        }
        atom.AddItem(new Item(static_cast<int64_t>(0xDEADDEAD)), &item);
        item.eraseFromParent();
        changed = true;
      }
    }
  }

  return changed;
}
