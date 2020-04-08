// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/simplify_trampoline.h"



// -----------------------------------------------------------------------------
const char *SimplifyTrampolinePass::kPassID = "simplify-trampoline";

// -----------------------------------------------------------------------------
const char *SimplifyTrampolinePass::GetPassName() const
{
  return "Trampoline Simplification";
}

// -----------------------------------------------------------------------------
void SimplifyTrampolinePass::Run(Prog *prog)
{
  for (auto it = prog->begin(); it != prog->end(); ) {
    Func *caller = &*it++;
    if (auto *callee = GetTarget(caller)) {
      llvm::SmallVector<Inst *, 8> callSites;

      // Check if all calls to the callee can be altered.
      bool CanReplace = true;
      for (User *callUser : callee->users()) {
        if (auto *movInst = ::dyn_cast_or_null<MovInst>(callUser)) {
          for (User *movUser : movInst->users()) {
            if (auto *inst = ::dyn_cast_or_null<Inst>(movUser)) {
              switch (inst->GetKind()) {
                case Inst::Kind::CALL: {
                  auto *call = static_cast<const CallInst *>(inst);
                  CanReplace = movInst == call->GetCallee();
                  break;
                }
                case Inst::Kind::TCALL: {
                  auto *call = static_cast<const TailCallInst *>(inst);
                  CanReplace = movInst == call->GetCallee();
                  break;
                }
                case Inst::Kind::INVOKE: {
                  auto *call = static_cast<const InvokeInst *>(inst);
                  CanReplace = movInst == call->GetCallee();
                  break;
                }
                case Inst::Kind::TINVOKE: {
                  auto *call = static_cast<const TailInvokeInst *>(inst);
                  CanReplace = movInst == call->GetCallee();
                  break;
                }
                default: {
                  CanReplace = false;
                  break;
                }
              }
              if (CanReplace) {
                callSites.push_back(inst);
                continue;
              }
            }
            CanReplace = false;
            break;
          }
          if (CanReplace) {
            continue;
          }
        }
        CanReplace = false;
        break;
      }

      if (CanReplace) {
        CallingConv conv = caller->GetCallingConv();

        for (Inst *call : callSites) {
           switch (call->GetKind()) {
            case Inst::Kind::CALL: {
              static_cast<CallInst *>(call)->SetCallingConv(conv);
              break;
            }
            case Inst::Kind::TCALL: {
              static_cast<TailCallInst *>(call)->SetCallingConv(conv);
              break;
            }
            case Inst::Kind::INVOKE: {
              static_cast<InvokeInst *>(call)->SetCallingConv(conv);
              break;
            }
            case Inst::Kind::TINVOKE: {
              static_cast<TailInvokeInst *>(call)->SetCallingConv(conv);
              break;
            }
            default:
              llvm_unreachable("invalid instruction kind");
          }
        }

        callee->SetCallingConv(conv);
        caller->replaceAllUsesWith(callee);
        caller->eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
Func *SimplifyTrampolinePass::GetTarget(Func *caller)
{
  // Linear control flow.
  if (caller->size() != 1) {
    return nullptr;
  }
  auto *block = &*caller->begin();

  // The function must return the result of a call.
  auto *ret = ::dyn_cast_or_null<ReturnInst>(block->GetTerminator());
  if (!ret) {
    return nullptr;
  }
  auto *call = ::dyn_cast_or_null<CallInst>(ret->GetValue());
  if (!call) {
    return nullptr;
  }

  // Arguments must be forwarded in order.
  unsigned I = 0;
  for (Inst *inst : call->args()) {
    if (auto *arg = ::dyn_cast_or_null<ArgInst>(inst)) {
      if (arg->GetIdx() != I) {
        return nullptr;
      }
      ++I;
      continue;
    }
    return nullptr;
  }

  // Function can have args + move + call +return.
  if (I + 3 != block->size()) {
    return nullptr;
  }

  // Find the callee.
  auto *mov = ::dyn_cast_or_null<MovInst>(call->GetCallee());
  auto *callee = ::dyn_cast_or_null<Func>(mov->GetArg());
  if (!callee) {
    return nullptr;
  }

  // Ensure calling conventions are compatible.
  if (call->GetCallingConv() != callee->GetCallingConv()) {
    return nullptr;
  }

  // Ensure functions are compatible.
  if (callee->IsVarArg() || caller->IsVarArg()) {
    return nullptr;
  }
  if (!callee->IsHidden() || !caller->IsHidden()) {
    return nullptr;
  }
  if (callee->params() != caller->params()) {
    return nullptr;
  }

  // Candidate for replacement.
  return callee;
}
