// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_eval.h"



// -----------------------------------------------------------------------------
SymbolicEval::SymbolicEval(SymbolicContext &ctx, TaintedObjects::Tainted &t)
{
}

// -----------------------------------------------------------------------------
void SymbolicEval::Evaluate(Inst *inst)
{
  return InstVisitor::Dispatch(inst);
}

// -----------------------------------------------------------------------------
void SymbolicEval::VisitMov(MovInst *i)
{
  Ref<Value> arg = i->GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      llvm_unreachable("INST");
    }
    case Value::Kind::GLOBAL: {
      const Global &g = *cast<Global>(arg);
      switch (g.GetKind()) {
        case Global::Kind::EXTERN: {
          llvm_unreachable("EXTERN");
        }
        case Global::Kind::FUNC: {
          llvm_unreachable("FUNC");
        }
        case Global::Kind::BLOCK: {
          llvm_unreachable("BLOCK");
        }
        case Global::Kind::ATOM: {
          llvm_unreachable("ATOM");
        }
      }
      llvm_unreachable("invalid global kind");
    }
    case Value::Kind::EXPR: {
      const Expr &e = *cast<Expr>(arg);
      switch (e.GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto &symOff = static_cast<const SymbolOffsetExpr &>(e);
          const Global *g = symOff.GetSymbol();
          switch (g->GetKind()) {
            case Global::Kind::EXTERN: {
              llvm_unreachable("EXTERN");
            }
            case Global::Kind::FUNC: {
              llvm_unreachable("FUNC");
            }
            case Global::Kind::BLOCK: {
              llvm_unreachable("BLOCK");
            }
            case Global::Kind::ATOM: {
              llvm_unreachable("ATOM");
            }
          }
          llvm_unreachable("invalid global kind");
        }
      }
      llvm_unreachable("invalid expression kind");
    }
    case Value::Kind::CONST: {
      llvm_unreachable("CONST");
    }
  }
  llvm_unreachable("invalid value kind");
}
