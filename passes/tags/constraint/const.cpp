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
void ConstraintSolver::VisitGetInst(GetInst &i)
{
  switch (i.GetReg()) {
    case Register::SP:
    case Register::FS:
    case Register::RET_ADDR:
    case Register::FRAME_ADDR: {
      return ExactlyPointer(i);
    }
    case Register::X86_CR0:
    case Register::X86_CR4: {
      return ExactlyInt(i);
    }
    case Register::X86_CR2:
    case Register::X86_CR3: {
      return ExactlyPointer(i);
    }
    case Register::X86_DS:
    case Register::X86_ES:
    case Register::X86_SS:
    case Register::X86_FS:
    case Register::X86_GS:
    case Register::X86_CS: {
      return ExactlyInt(i);
    }
    case Register::AARCH64_FPSR:
    case Register::AARCH64_FPCR:
    case Register::AARCH64_CNTVCT:
    case Register::AARCH64_CNTFRQ:
    case Register::AARCH64_FAR:
    case Register::AARCH64_VBAR: {
      return ExactlyInt(i);
    }
    case Register::RISCV_FFLAGS:
    case Register::RISCV_FRM:
    case Register::RISCV_FCSR: {
      return ExactlyInt(i);
    }
    case Register::PPC_FPSCR: {
      return ExactlyInt(i);
    }
  }
  llvm_unreachable("invalid register kind");
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitUndefInst(UndefInst &i)
{
  AtMost(i, ConstraintType::PTR_INT);
  AtLeast(i, ConstraintType::BOT);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitSyscallInst(SyscallInst &i)
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
      Infer(i);
      auto ai = ::cast<Inst>(arg);
      if (analysis_.Find(ai) <= analysis_.Find(i)) {
        Subset(ai, i);
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
