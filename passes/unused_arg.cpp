// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/unused_arg.h"

#define DEBUG_TYPE "unused-arg"

STATISTIC(NumFuncsSimplified, "Simplified functions");
STATISTIC(NumSitesSimplified, "Call sites with eliminated arguments");
STATISTIC(NumSitesReplaced, "Call sites with replaced arguments");


// -----------------------------------------------------------------------------
const char *UnusedArgPass::kPassID = "unused-arg";



// -----------------------------------------------------------------------------
bool UnusedArgPass::Run(Prog &prog)
{
  bool changed = false;

  std::unordered_map<Func *, std::set<unsigned>> usedArgs;
  std::unordered_map<Func *, std::set<unsigned>> removedArgs;
  for (Func &func : prog) {
    std::set<unsigned> used;
    for (Block &block : func) {
      for (Inst &inst : block) {
        if (auto arg = ::cast_or_null<ArgInst>(&inst)) {
          used.insert(arg->GetIndex());
        }
      }
    }
    const auto &params = func.params();
    std::vector<FlaggedType> newParams;
    if (params.size() != used.size()) {
      if (func.HasAddressTaken() || !func.IsLocal()) {
        usedArgs[&func] = std::move(used);
      } else {
        NumFuncsSimplified++;
        std::map<unsigned, unsigned> reindex;
        std::set<unsigned> &removed = removedArgs[&func];
        for (unsigned i = 0; i < params.size(); ++i) {
          if (used.count(i)) {
            reindex[i] = reindex.size();
            newParams.push_back(params[i]);
          } else {
            removed.insert(i);
          }
        }
        func.SetParameters(newParams);
        for (Block &block : func) {
          for (auto it = block.begin(); it != block.end(); ) {
            if (auto *arg = ::cast_or_null<ArgInst>(&*it++)) {
              auto *newArg = new ArgInst(
                  arg->GetType(),
                  reindex[arg->GetIndex()],
                  arg->GetAnnots()
              );
              block.AddInst(newArg, arg);
              arg->replaceAllUsesWith(newArg);
              arg->eraseFromParent();
            }
          }
        }
      }
    }
  }

  for (Func &func : prog) {
    for (Block &block : func) {
      auto *site = ::cast_or_null<CallSite>(block.GetTerminator());
      if (!site) {
        continue;
      }
      auto *callee = site->GetDirectCallee();
      if (!callee) {
        continue;
      }

      bool replaced = false;
      std::vector<Ref<Inst>> newArgs;
      std::vector<TypeFlag> newFlags;
      if (auto it = usedArgs.find(callee); it != usedArgs.end()) {
        for (unsigned i = 0, n = site->arg_size(); i < n; ++i) {
          newFlags.push_back(site->flag(i));
          auto arg = site->arg(i);
          if (it->second.count(i)) {
            newArgs.push_back(arg);
          } else if (!arg->Is(Inst::Kind::UNDEF)) {
            auto *undef = new UndefInst(arg.GetType(), {});
            block.AddInst(undef, site);
            newArgs.push_back(undef);
            replaced = true;
          }
        }
        if (replaced) {
          NumSitesReplaced++;
        }
      }
      if (auto it = removedArgs.find(callee); it != removedArgs.end()) {
        for (unsigned i = 0, n = site->arg_size(); i < n; ++i) {
          if (!it->second.count(i)) {
            newArgs.push_back(site->arg(i));
            newFlags.push_back(site->flag(i));
          }
        }
        replaced = true;
        NumSitesSimplified++;
      }

      if (replaced) {
        CallSite *newInst = nullptr;
        switch (site->GetKind()) {
          default: llvm_unreachable("not a call instruction");
          case Inst::Kind::CALL: {
            auto *call = static_cast<CallInst *>(site);
            newInst = new CallInst(
                std::vector<Type>{ site->type_begin(), site->type_end() },
                site->GetCallee(),
                newArgs,
                newFlags,
                call->GetCont(),
                call->GetCallingConv(),
                call->GetNumFixedArgs(),
                call->GetAnnots()
            );
            break;
          }
          case Inst::Kind::TAIL_CALL: {
            auto *call = static_cast<TailCallInst *>(site);
            newInst = new TailCallInst(
                std::vector<Type>{ site->type_begin(), site->type_end() },
                site->GetCallee(),
                newArgs,
                newFlags,
                call->GetCallingConv(),
                call->GetNumFixedArgs(),
                call->GetAnnots()
            );
            break;
          }
          case Inst::Kind::INVOKE: {
            auto *call = static_cast<InvokeInst *>(site);
            newInst = new InvokeInst(
                std::vector<Type>{ site->type_begin(), site->type_end() },
                site->GetCallee(),
                newArgs,
                newFlags,
                call->GetCont(),
                call->GetThrow(),
                call->GetCallingConv(),
                call->GetNumFixedArgs(),
                call->GetAnnots()
            );
            break;
          }
        }
        block.AddInst(newInst, site);
        site->replaceAllUsesWith(newInst);
        site->eraseFromParent();
        changed = true;
      }
    }
  }

  return changed;
}

// -----------------------------------------------------------------------------
const char *UnusedArgPass::GetPassName() const
{
  return "Unused Argument Elimination";
}
