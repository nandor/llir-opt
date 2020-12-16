// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <stack>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "passes/init_unroll.h"
#include "passes/inliner/inline_helper.h"
#include "passes/inliner/inline_util.h"
#include "passes/inliner/trampoline_graph.h"



// -----------------------------------------------------------------------------
static unsigned CountDirectUses(Func *func)
{
  unsigned codeUses = 0;
  for (const User *user : func->users()) {
    if (auto *inst = ::cast_or_null<const Inst>(user)) {
      if (auto *movInst = ::cast_or_null<const MovInst>(inst)) {
        for (const User *movUsers : movInst->users()) {
          codeUses++;
        }
      }
    }
  }
  return codeUses;
}

// -----------------------------------------------------------------------------
const char *InitUnrollPass::kPassID = "init-unroll";

// -----------------------------------------------------------------------------
static std::pair<unsigned, unsigned> CountUses(const Func *func)
{
  unsigned dataUses = 0, codeUses = 0;
  for (const User *user : func->users()) {
    if (auto *inst = ::cast_or_null<const Inst>(user)) {
      if (auto *movInst = ::cast_or_null<const MovInst>(inst)) {
        for (const User *movUsers : movInst->users()) {
          codeUses++;
        }
      } else {
        codeUses++;
      }
    } else {
      dataUses++;
    }
  }
  return { dataUses, codeUses };
}

// -----------------------------------------------------------------------------
static bool ShouldInline(const Func *f)
{
  auto [data, code] = CountUses(f);
  if (code == 1) {
    return true;
  }

  unsigned copies = (data ? 1 : 0) + code;
  if (copies * f->inst_size() < 100) {
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
void InitUnrollPass::Run(Prog *prog)
{
  auto &cfg = GetConfig();
  if (!cfg.Static || cfg.Entry.empty()) {
    return;
  }

  if (auto *g = ::cast<Func>(prog->GetGlobal(cfg.Entry))) {
    TrampolineGraph tg(prog);

    auto it = g->begin();
    while (it != g->end()) {
      auto *call = ::cast_or_null<CallSite>(it->GetTerminator());
      if (!call) {
        ++it;
        continue;
      }
      auto mov = ::cast_or_null<MovInst>(call->GetCallee());
      if (!mov) {
        ++it;
        continue;
      }
      auto *caller = it->getParent();
      auto callee = ::cast_or_null<Func>(mov->GetArg()).Get();
      if (!callee || !CanInline(caller, callee) || !ShouldInline(callee)) {
        ++it;
        continue;
      }

      switch (call->GetKind()) {
        default: llvm_unreachable("not a call site");
        case Inst::Kind::CALL: {
          InlineHelper(static_cast<CallInst *>(call), callee, tg).Inline();
          break;
        }
        case Inst::Kind::TAIL_CALL: {
          InlineHelper(static_cast<TailCallInst *>(call), callee, tg).Inline();
          break;
        }
        case Inst::Kind::INVOKE: {
          ++it;
          continue;
        }
      }
      if (mov->use_empty()) {
        mov->eraseFromParent();
      }
      if (callee->use_empty()) {
        callee->eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *InitUnrollPass::GetPassName() const
{
  return "Initialisation unrolling";
}
