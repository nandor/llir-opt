// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>
#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/tail_rec_elim.h"



// -----------------------------------------------------------------------------
const char *TailRecElimPass::kPassID = "tail-rec-elim";

// -----------------------------------------------------------------------------
void TailRecElimPass::Run(Prog *prog)
{
  for (Func &f : *prog) {
    if (!f.IsVarArg()) {
      Run(f);
    }
  }
}

// -----------------------------------------------------------------------------
const char *TailRecElimPass::GetPassName() const
{
  return "Tail Recursion Elimination";
}

// -----------------------------------------------------------------------------
void TailRecElimPass::Run(Func &func)
{
  Block *header = nullptr;
  llvm::DenseMap<unsigned, PhiInst *> phis;
  llvm::DenseMap<unsigned, llvm::SmallVector<Ref<ArgInst>, 2>> args;

  Block *entry = &func.getEntryBlock();
  for (Block &block : func) {
    for (auto it = block.begin(); it != block.end(); ) {
      auto *inst = &*it++;
      if (auto *arg = ::cast_or_null<ArgInst>(inst)) {
        args[arg->GetIdx()].push_back(arg);
        continue;
      }
      if (auto *call = ::cast_or_null<TailCallInst>(inst)) {
        if (Ref<MovInst> movInstRef = ::cast_or_null<MovInst>(call->GetCallee())) {
          if (Ref<Func> callee = ::cast_or_null<Func>(movInstRef->GetArg())) {
            if (&*callee == &func) {
              if (!header) {
                // Create a header block before entry.
                std::ostringstream os;
                os << ".L" << func.GetName() << "$tail_entry";
                header = new Block(os.str());
                func.insert(entry->getIterator(), header);
                header->AddInst(new JumpInst(entry, {}));

                // Add arg instructions to that block and phis to the following.
                unsigned i = 0;
                for (FlaggedType param : func.params()) {
                  auto ty = param.GetType();
                  auto *arg = new ArgInst(ty, i, {});
                  header->AddInst(arg, &*header->rbegin());

                  auto *phi = new PhiInst(ty, {});
                  phi->Add(header, arg);
                  entry->AddPhi(phi);

                  phis[i++] = phi;
                }
              }

              Block *from = call->getParent();
              unsigned i = 0;
              for (Ref<Inst> arg : call->args()) {
                phis[i++]->Add(from, arg);
              }

              from->AddInst(new JumpInst(entry, {}), call);
              call->eraseFromParent();
            }
          }
        }
        continue;
      }
    }
  }

  if (header) {
    // Remove the old arguments.
    for (auto &[idx, argInsts] : args) {
      for (Ref<ArgInst> argRef : argInsts) {
        auto *arg = argRef.Get();
        arg->replaceAllUsesWith(phis[idx]);
        arg->eraseFromParent();
      }
    }
  }
}
