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

        llvm::SmallVector<Type, 5> types;
        llvm::SmallVector<Inst *, 5> args;
        Inst *callee = nullptr;
        switch (inst->GetKind()) {
          case Inst::Kind::TCALL:
          case Inst::Kind::CALL: {
            auto *call = static_cast<CallSite *>(inst);
            callee = call->GetCallee();
            types = llvm::SmallVector<Type, 5>{ call->type_begin(), call->type_end() };
            args = llvm::SmallVector<Inst *, 5>{ call->arg_begin(), call->arg_end() };
            break;
          }
          default: {
            continue;
          }
        }

        auto GetArgF64 = [&types, &args, inst] () -> Inst * {
          if (types.size() != 1 || types[0] != Type::F64)
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
          Block *parent = inst->getParent();
          switch (inst->GetKind()) {
            case Inst::Kind::CALL: {
              auto *cont = static_cast<CallInst *>(inst)->GetCont();
              parent->AddInst(newInst, inst);
              parent->AddInst(new JumpInst(cont, {}));
              inst->replaceAllUsesWith(newInst);
              inst->eraseFromParent();
              break;
            }
            case Inst::Kind::TCALL: {
              assert(inst->use_empty() && "tail call should have no users");
              parent->AddInst(newInst, inst);
              parent->AddInst(new ReturnInst(newInst, {}));
              inst->eraseFromParent();
              break;
            }
            default: {
              llvm_unreachable("invalid instruction");
            }
          }
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
