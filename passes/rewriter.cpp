// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/rewriter.h"



// -----------------------------------------------------------------------------
const char *RewriterPass::kPassID = "rewriter";

// -----------------------------------------------------------------------------
void RewriterPass::Run(Prog *prog)
{
  for (Func &func : *prog) {
    for (Block &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        Inst *inst = &*it++;

        std::optional<Type> type;
        llvm::SmallVector<Inst *, 5> args;
        Inst *callee = nullptr;
        switch (inst->GetKind()) {
          case Inst::Kind::CALL: {
            auto *call = static_cast<CallInst *>(inst);
            callee = call->GetCallee();
            type = call->GetType();
            args = llvm::SmallVector<Inst *, 5>{ call->arg_begin(), call->arg_end() };
            break;
          }
          case Inst::Kind::INVOKE:
          case Inst::Kind::TCALL: {
            callee = static_cast<CallSite<TerminatorInst> *>(inst)->GetCallee();
            break;
          }
          default: {
            continue;
          }
        }

        auto GetArgF64 = [&type, &args, inst] () -> Inst * {
          if (type != Type::F64)
            return nullptr;
          if (args.size() != 1)
            return nullptr;
          if (args[0]->GetType(0) != Type::F64)
            return nullptr;
          return args[0];
        };

        auto GetAnnot = [inst] {
          AnnotSet annots = inst->GetAnnots();
          annots.Clear<CamlFrame>();
          return annots;
        };

        Inst *newInst = nullptr;
        if (auto *mov = ::dyn_cast_or_null<MovInst>(callee)) {
          if (auto *ext = ::dyn_cast_or_null<Extern>(mov->GetArg())) {
            if (auto *arg = GetArgF64()) {
              if (ext->getName() == "cos") {
                newInst = new CosInst(Type::F64, arg, GetAnnot());
              } else if (ext->getName() == "exp") {
                newInst = new ExpInst(Type::F64, arg, GetAnnot());
              } else if (ext->getName() == "sin") {
                newInst = new SinInst(Type::F64, arg, GetAnnot());
              } else if (ext->getName() == "sqrt") {
                newInst = new SqrtInst(Type::F64, arg, GetAnnot());
              }
            }
          }
        }

        if (newInst) {
          inst->getParent()->AddInst(newInst, inst);
          inst->replaceAllUsesWith(newInst);
          inst->eraseFromParent();
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *RewriterPass::GetPassName() const
{
  return "Extern Rewriter";
}
