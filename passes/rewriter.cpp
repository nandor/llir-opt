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
        if (auto *call = ::cast_or_null<CallSite>(&*it++)) {
          auto GetArgF64 = [call] () -> Ref<Inst> {
            if (call->type_size() != 1 || call->arg_size() != 1) {
              return nullptr;
            }
            if (call->type(0) != Type::F64) {
              return nullptr;
            }
            if (call->arg(0).GetType() != Type::F64) {
              return nullptr;
            }
            return call->arg(0);
          };

          auto GetAnnot = [call] () -> AnnotSet {
            AnnotSet annots = call->GetAnnots();
            annots.Clear<CamlFrame>();
            return annots;
          };

          Inst *newInst = nullptr;
          if (Ref<MovInst> movRef = ::cast_or_null<MovInst>(call->GetCallee())) {
            if (Ref<Extern> extRef = ::cast_or_null<Extern>(movRef->GetArg())) {
              if (Ref<Inst> arg = GetArgF64()) {
                if (extRef->getName() == "cos") {
                  newInst = new CosInst(Type::F64, arg, GetAnnot());
                } else if (extRef->getName() == "exp") {
                  newInst = new ExpInst(Type::F64, arg, GetAnnot());
                } else if (extRef->getName() == "sin") {
                  newInst = new SinInst(Type::F64, arg, GetAnnot());
                } else if (extRef->getName() == "sqrt") {
                  newInst = new SqrtInst(Type::F64, arg, GetAnnot());
                }
              }
            }
          }

          if (newInst) {
            Block *parent = call->getParent();
            switch (call->GetKind()) {
              case Inst::Kind::CALL: {
                Block *cont = static_cast<CallInst *>(call)->GetCont();
                parent->AddInst(newInst, call);
                parent->AddInst(new JumpInst(cont, {}));
                call->replaceAllUsesWith(newInst);
                call->eraseFromParent();
                break;
              }
              case Inst::Kind::TCALL: {
                assert(call->use_empty() && "tail call should have no users");
                parent->AddInst(newInst, call);
                parent->AddInst(new ReturnInst({ newInst }, {}));
                call->eraseFromParent();
                break;
              }
              case Inst::Kind::INVOKE: {
                Block *cont = static_cast<InvokeInst *>(call)->GetCont();
                parent->AddInst(newInst, call);
                parent->AddInst(new JumpInst(cont, {}));
                call->replaceAllUsesWith(newInst);
                call->eraseFromParent();
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
}

// -----------------------------------------------------------------------------
const char *RewriterPass::GetPassName() const
{
  return "Extern Rewriter";
}
