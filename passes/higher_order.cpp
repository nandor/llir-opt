// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/higher_order.h"



// -----------------------------------------------------------------------------
void HigherOrderPass::Run(Prog *prog)
{
  for (auto &func : *prog) {
    llvm::DenseSet<unsigned> args;
    for (auto &block : func) {
      for (auto &inst : block) {
        Inst *callee = nullptr;
        switch (inst.GetKind()) {
          case Inst::Kind::CALL: {
            callee = static_cast<CallInst *>(&inst)->GetCallee();
            break;
          }
          case Inst::Kind::INVOKE: {
            callee = static_cast<InvokeInst *>(&inst)->GetCallee();
            break;
          }
          case Inst::Kind::TCALL: {
            callee = static_cast<TailCallInst *>(&inst)->GetCallee();
            break;
          }
          case Inst::Kind::TINVOKE: {
            callee = static_cast<TailInvokeInst *>(&inst)->GetCallee();
            break;
          }
          default: {
            continue;
          }
        }
        if (callee->Is(Inst::Kind::ARG)) {
          args.insert(static_cast<ArgInst *>(callee)->GetIdx());
        }
      }
    }
    if (args.empty()) {
      continue;
    }
  }
}

// -----------------------------------------------------------------------------
const char *HigherOrderPass::GetPassName() const
{
  return "Higher Order Specialisation";
}
