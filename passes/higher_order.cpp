// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <map>
#include <set>

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/higher_order.h"



// -----------------------------------------------------------------------------
static Inst *GetCallee(Inst *inst)
{
  switch (inst->GetKind()) {
    case Inst::Kind::CALL: {
      return static_cast<CallInst *>(inst)->GetCallee();
    }
    case Inst::Kind::INVOKE: {
      return static_cast<InvokeInst *>(inst)->GetCallee();
    }
    case Inst::Kind::TCALL: {
      return static_cast<TailCallInst *>(inst)->GetCallee();
    }
    case Inst::Kind::TINVOKE: {
      return static_cast<TailInvokeInst *>(inst)->GetCallee();
    }
    default: {
      return nullptr;
    }
  }
}

// -----------------------------------------------------------------------------
void HigherOrderPass::Run(Prog *prog)
{
  // Identify simple higher order functions - those which invoke an argument.
  std::unordered_map<Func *, llvm::DenseSet<ArgInst *>> higherOrderFuncs;
  for (auto &func : *prog) {
    // Find arguments which reach a call site.
    llvm::DenseSet<ArgInst *> args;
    for (auto &block : func) {
      for (auto &inst : block) {
        if (Inst *callee = GetCallee(&inst)) {
          if (callee->Is(Inst::Kind::ARG)) {
            args.insert(static_cast<ArgInst *>(callee));
          }
        }
      }
    }

    if (args.empty()) {
      continue;
    }

    // Arguments should only be invoked, they should not escape.
    bool escapes = false;
    for (auto *arg : args) {
      for (auto *user : arg->users()) {
        if (auto *inst = ::dyn_cast_or_null<Inst>(user)) {
          Inst *callee = GetCallee(inst);
          if (callee != arg) {
            escapes = true;
            break;
          }
        } else {
          escapes = true;
          break;
        }
      }
    }

    // Candidate function, along with HO arguments.
    if (!escapes) {
      higherOrderFuncs.emplace(&func, std::move(args));
    }
  }


  // Find the call sites of these HOFs, identifying arguments.
  std::map<std::pair<Func *, std::vector<Func *>>, std::set<Inst *>> sites;
  for (const auto &[func, args] : higherOrderFuncs) {
    for (auto *funcUser : func->users()) {
      if (auto *movInst = ::dyn_cast_or_null<MovInst>(funcUser)) {
        for (auto *movUser : movInst->users()) {
          if (auto *inst = ::dyn_cast_or_null<Inst>(movUser)) {
            // Find the arguments to the call of the higher-order function.
            std::vector<Inst *> actualArgs;
            switch (inst->GetKind()) {
              case Inst::Kind::CALL: {
                auto *call = static_cast<CallInst *>(inst);
                if (call->GetCallee() == movInst) {
                  std::copy(
                      call->arg_begin(), call->arg_end(),
                      std::back_inserter(actualArgs)
                  );
                }
                break;
              }
              case Inst::Kind::INVOKE:
              case Inst::Kind::TCALL:
              case Inst::Kind::TINVOKE: {
                auto *call = static_cast<CallSite<TerminatorInst> *>(inst);
                if (call->GetCallee() == movInst) {
                  std::copy(
                      call->arg_begin(), call->arg_end(),
                      std::back_inserter(actualArgs)
                  );
                }
                break;
              }
              default: {
                // Nothing to specialise here.
                break;
              }
            }

            // Check for function arguments.
            bool specialise = !actualArgs.empty();
            std::vector<Func *> params;
            for (ArgInst *arg : args) {
              const unsigned i = arg->GetIdx();
              if (i < actualArgs.size()) {
                if (auto *inst = ::dyn_cast_or_null<MovInst>(actualArgs[i])) {
                  if (auto *func = ::dyn_cast_or_null<Func>(inst->GetArg())) {
                    params.push_back(func);
                    continue;
                  }
                }
              }

              specialise = false;
              break;
            }

            // Record the specialisation site.
            if (specialise) {
              sites.emplace(
                  std::make_pair(func, params),
                  std::set<Inst *>{}
              ).first->second.insert(inst);
            }
          }
        }
      }
    }
  }

  // Check if the function is worth specialising and specialise it.
  for (const auto &[key, insts] : sites) {
    const auto &[func, params] = key;

    // Only specialise single-arg HOFs.
    if (params.size() != 1) {
      continue;
    }
    auto *param = params[0];

    // Only specialise if all the uses of the func are among the sites.
    bool specialise = true;
    for (auto *funcUser : param->users()) {
      if (auto *movInst = ::dyn_cast_or_null<MovInst>(funcUser)) {
        bool valid = true;
        for (auto *movUser : movInst->users()) {
          if (auto *inst = ::dyn_cast_or_null<Inst>(movUser)) {
            if (insts.count(inst) != 0) {
              continue;
            }
          }
          valid = false;
          break;
        }

        if (valid) {
          continue;
        }
      }

      specialise = false;
      break;
    }

    if (specialise) {
      // TODO: specialise here
    }
  }
}

// -----------------------------------------------------------------------------
const char *HigherOrderPass::GetPassName() const
{
  return "Higher Order Specialisation";
}
