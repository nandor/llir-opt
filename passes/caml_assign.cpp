// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Statistic.h>
#include <llvm/Support/Debug.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/caml_assign.h"

#define DEBUG_TYPE "caml-assign"

STATISTIC(NumCallsFolded, "Number caml_initialize calls removed");



// -----------------------------------------------------------------------------
const char *CamlAssignPass::kPassID = "caml-assign";

// -----------------------------------------------------------------------------
static bool IsStatic(Ref<Value> value)
{
  switch (value->GetKind()) {
    case Value::Kind::CONST: {
      // Constants cannot be heap pointers.
      return true;
    }
    case Value::Kind::INST: {
      // Arbitrary value, function not turned into store.
      return false;
    }
    case Value::Kind::GLOBAL:
    case Value::Kind::EXPR: {
      // Static data, function turned into store.
      return true;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool CamlAssignPass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    for (Block &block : func) {
      auto *site = ::cast_or_null<CallSite>(block.GetTerminator());
      if (!site || site->arg_size() != 2 || site->type_size() != 0) {
        continue;
      }
      auto *callee = site->GetDirectCallee();
      if (!callee) {
        continue;
      }
      auto name = callee->getName();
      if (name != "caml_initialize") {
        continue;
      }
      auto val = ::cast_or_null<MovInst>(site->arg(1));
      if (!val || !IsStatic(val->GetArg())) {
        continue;
      }
      LLVM_DEBUG(llvm::dbgs()
          << "Simplifying " << name << " in " << block.getName() << "\n"
      );
      NumCallsFolded++;
      auto *store = new StoreInst(site->arg(0), site->arg(1), {});
      block.AddInst(store, site);
      switch (site->GetKind()) {
        default: llvm_unreachable("not a call");
        case Inst::Kind::CALL: {
          auto *call = static_cast<CallInst *>(site);
          block.AddInst(new JumpInst(call->GetCont(), {}), site);
          break;
        }
        case Inst::Kind::INVOKE: {
          auto *invoke = static_cast<InvokeInst *>(site);
          block.AddInst(new JumpInst(invoke->GetCont(), {}), site);
          auto *handler = invoke->GetThrow();
          for (PhiInst &phi : handler->phis()) {
            phi.Remove(&block);
          }
          break;
        }
        case Inst::Kind::TAIL_CALL: {
          block.AddInst(new ReturnInst({}, {}), site);
          break;
        }
      }
      site->eraseFromParent();
      changed = true;
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *CamlAssignPass::GetPassName() const
{
  return "caml_modify/caml_initialize Simplification";
}
