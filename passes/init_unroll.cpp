// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <stack>
#include <queue>

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
const char *InitUnrollPass::kPassID = "init-unroll";



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
bool InitUnrollPass::ShouldInline(const CallSite *call, const Func *f)
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

  std::queue<Func *> q;
  if (auto *entry = ::cast<Func>(prog->GetGlobal(cfg.Entry))) {
    // Start inlining methods into the entry point of the program.
    TrampolineGraph tg(prog);

    q.push(entry);
    while (!q.empty()) {
      Func *caller = q.front();
      q.pop();

      auto it = caller->begin();
      while (it != caller->end()) {
        // Find call instructions with a known call site.
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
        auto callee = ::cast_or_null<Func>(mov->GetArg()).Get();
        if (!callee) {
          ++it;
          continue;
        }

        // Do not inline if illegal or expensive. If the callee is a method
        // with a single use, it can be assumed it is on the initialisation
        // pass, thus this conservative inlining pass continue with it.
        if (!CanInline(caller, callee) || !ShouldInline(call, callee)) {
          if (callee->use_size() == 1) {
            q.push(callee);
          }
          ++it;
          continue;
        }

        InlineHelper(call, callee, tg).Inline();

        if (mov->use_empty()) {
          mov->eraseFromParent();
        }
        if (callee->use_empty()) {
          callee->eraseFromParent();
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *InitUnrollPass::GetPassName() const
{
  return "Initialisation Unrolling";
}
