// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>
#include <set>

#include <llvm/ADT/SCCIterator.h>

#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/analysis/call_graph.h"
#include "core/analysis/init_path.h"



// -----------------------------------------------------------------------------
InitPath::InitPath(Prog &prog, Func *entry)
{
  std::function<void (Func &)> taint =
    [&, this] (Func &func)
    {
      if (!loop_.insert(&func.getEntryBlock()).second) {
        return;
      }

      for (Block &block : func) {
        loop_.insert(&block);
        if (auto *call = ::cast_or_null<CallSite>(block.GetTerminator())) {
          if (auto *callee = call->GetDirectCallee()) {
            taint(*callee);
          }
        }
      }
    };

  CallGraph cg(prog);
  for (auto it = llvm::scc_begin(&cg); !it.isAtEnd(); ++it) {
    if (it->size() > 1 || (*it)[0]->IsRecursive()) {
      for (auto *node : *it) {
        if (auto *f = node->GetCaller()) {
          taint(*f);
        }
      }
    } else {
      if (auto *f = (*it)[0]->GetCaller()) {
        if (!f->IsLocal() || f->HasAddressTaken()) {
          taint(*f);
        }
      }
    }
  }

  if (entry) {
    std::queue<Func *> q;
    q.push(entry);

    while (!q.empty()) {
      Func &func = *q.front();
      q.pop();
      if (loop_.count(&func.getEntryBlock())) {
        continue;
      }

      for (auto it = llvm::scc_begin(&func); !it.isAtEnd(); ++it) {
        std::set<Block *> scc(it->begin(), it->end());
        bool isLoop = false;
        if (scc.size() > 1) {
          isLoop = true;
        } else {
          for (Block *b : scc) {
            for (Block *succ : b->successors()) {
              if (scc.count(succ)) {
                isLoop = true;
                break;
              }
            }
          }
        }
        for (Block *b : scc) {
          if (isLoop) {
            loop_.insert(b);
          }
          if (auto *call = ::cast_or_null<CallSite>(b->GetTerminator())) {
            if (auto *f = call->GetDirectCallee()) {
              if (isLoop) {
                taint(*f);
              } else {
                q.push(f);
              }
            }
          }
        }
      }
    }
  }
}
