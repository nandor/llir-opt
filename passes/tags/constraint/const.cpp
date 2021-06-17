// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/data.h"
#include "core/cast.h"
#include "core/object.h"
#include "passes/tags/constraints.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
void ConstraintSolver::VisitUndefInst(UndefInst &i)
{
  AtMost(i, ConstraintType::PTR_INT);
  AtLeast(i, ConstraintType::BOT);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitMovInst(MovInst &i)
{
  auto global = [this, &i](ConstRef<Global> g)
  {
    switch (g->GetKind()) {
      case Global::Kind::EXTERN: {
        return AnyPointer(i);
      }
      case Global::Kind::FUNC: {
        return ExactlyFunc(i);
      }
      case Global::Kind::BLOCK: {
        return ExactlyPointer(i);
      }
      case Global::Kind::ATOM: {
        auto *section = ::cast<Atom>(g)->getParent()->getParent();
        if (section->getName() == ".data.caml") {
          return ExactlyHeap(i);
        } else {
          return ExactlyPointer(i);
        }
      }
    }
    llvm_unreachable("invalid global kind");
  };

  auto arg = i.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      auto ai = ::cast<Inst>(arg);
      auto ty = analysis_.Find(i);
      if (ty <= analysis_.Find(ai)) {
        Subset(i, ai);
        AtMostInfer(i, ty);
      } else {
        Infer(i);
      }
      return;
    }
    case Value::Kind::GLOBAL: {
      return global(::cast<Global>(arg));
    }
    case Value::Kind::EXPR: {
      switch (::cast<Expr>(arg)->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          return global(::cast<SymbolOffsetExpr>(arg)->GetSymbol());
        }
      }
      llvm_unreachable("invalid expression kind");
    }
    case Value::Kind::CONST: {
      return ExactlyInt(i);
    }
  }
  llvm_unreachable("invalid value kind");
}
