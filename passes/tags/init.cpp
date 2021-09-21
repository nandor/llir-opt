// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/object.h"
#include "core/data.h"
#include "passes/tags/init.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
void Init::VisitArgInst(ArgInst &i)
{
  auto &func = *i.getParent()->getParent();
  switch (func.GetCallingConv()) {
    case CallingConv::C:
    case CallingConv::SETJMP:
    case CallingConv::XEN:
    case CallingConv::INTR:
    case CallingConv::MULTIBOOT:
    case CallingConv::WIN64: {
      if (func.IsRoot() || func.HasAddressTaken()) {
        analysis_.Mark(i, TaggedType::PtrInt());
      }
      return;
    }
    case CallingConv::CAML: {
      if (target_) {
        switch (target_->GetKind()) {
          case Target::Kind::X86: {
            switch (i.GetIndex()) {
              case 0: {
                analysis_.Mark(i, TaggedType::Ptr());
                return;
              }
              case 1: {
                analysis_.Mark(i, TaggedType::Young());
                return;
              }
              default: {
                if (func.HasAddressTaken() || !func.IsLocal()) {
                  analysis_.Mark(i, Infer(i.GetType()));
                  return;
                } else {
                  return;
                }
              }
            }
            llvm_unreachable("invalid argument index");
          }
          case Target::Kind::PPC: {
            llvm_unreachable("not implemented");
          }
          case Target::Kind::AARCH64: {
            llvm_unreachable("not implemented");
          }
          case Target::Kind::RISCV: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid target kind");
      } else {
        llvm_unreachable("not implemented");
      }
    }
    case CallingConv::CAML_ALLOC: {
      if (target_) {
        switch (target_->GetKind()) {
          case Target::Kind::X86: {
            switch (i.GetIndex()) {
              case 0: {
                analysis_.Mark(i, TaggedType::Ptr());
                return;
              }
              case 1: {
                analysis_.Mark(i, TaggedType::Young());
                return;
              }
              default: {
                llvm_unreachable("invalid argument");
              }
            }
            llvm_unreachable("invalid argument index");
          }
          case Target::Kind::PPC: {
            llvm_unreachable("not implemented");
          }
          case Target::Kind::AARCH64: {
            llvm_unreachable("not implemented");
          }
          case Target::Kind::RISCV: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid target kind");
      } else {
        llvm_unreachable("not implemented");
      }
    }
    case CallingConv::CAML_GC: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid calling convention");
}

// -----------------------------------------------------------------------------
void Init::VisitMovInst(MovInst &i)
{
  auto global = [this, &i](ConstRef<Global> g)
  {
    switch (g->GetKind()) {
      case Global::Kind::EXTERN: {
        analysis_.Mark(i, TaggedType::Ptr());
        return;
      }
      case Global::Kind::FUNC: {
        analysis_.Mark(i, TaggedType::Func());
        return;
      }
      case Global::Kind::BLOCK: {
        analysis_.Mark(i, TaggedType::Ptr());
        return;
      }
      case Global::Kind::ATOM: {
        auto *section = ::cast<Atom>(g)->getParent()->getParent();
        if (section->getName() == ".data.caml") {
          analysis_.Mark(i, TaggedType::Heap());
        } else {
          analysis_.Mark(i, TaggedType::Ptr());
        }
        return;
      }
    }
    llvm_unreachable("invalid global kind");
  };

  auto arg = i.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
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
      switch (::cast<Constant>(arg)->GetKind()) {
        case Constant::Kind::INT: {
          const auto &v = ::cast<ConstantInt>(arg)->GetValue();
          if (v.getBitWidth() <= sizeof(int64_t) * CHAR_BIT) {
            analysis_.Mark(i, TaggedType::Const(v.getSExtValue()));
          } else {
            analysis_.Mark(i, TaggedType::Int());
          }
          return;
        }
        case Constant::Kind::FLOAT: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
      }
      llvm_unreachable("invalid constant kind");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Init::VisitFrameInst(FrameInst &i)
{
  analysis_.Mark(i, TaggedType::Ptr());
}

// -----------------------------------------------------------------------------
void Init::VisitAllocaInst(AllocaInst &i)
{
  analysis_.Mark(i, TaggedType::Ptr());
}

// -----------------------------------------------------------------------------
void Init::VisitGetInst(GetInst &i)
{
  switch (i.GetReg()) {
    case Register::SP:
    case Register::FS:
    case Register::RET_ADDR:
    case Register::FRAME_ADDR: {
      analysis_.Mark(i, TaggedType::Ptr());
      return;
    }
    case Register::X86_CR0:
    case Register::X86_CR2:
    case Register::X86_CR3:
    case Register::X86_CR4: {
      llvm_unreachable("not implemented");
    }
    case Register::X86_DS:
    case Register::X86_ES:
    case Register::X86_SS:
    case Register::X86_FS:
    case Register::X86_GS:
    case Register::X86_CS: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
    case Register::AARCH64_FPSR:
    case Register::AARCH64_FPCR:
    case Register::AARCH64_CNTVCT:
    case Register::AARCH64_CNTFRQ:
    case Register::AARCH64_FAR:
    case Register::AARCH64_VBAR: {
      llvm_unreachable("not implemented");
    }
    case Register::RISCV_FFLAGS:
    case Register::RISCV_FRM:
    case Register::RISCV_FCSR: {
      llvm_unreachable("not implemented");
    }
    case Register::PPC_FPSCR: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid register kind");
}

// -----------------------------------------------------------------------------
void Init::VisitUndefInst(UndefInst &i)
{
  analysis_.Mark(i, TaggedType::Undef());
}

// -----------------------------------------------------------------------------
void Init::VisitCopySignInst(CopySignInst &i)
{
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Init::VisitFloatInst(FloatInst &i)
{
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Init::VisitX86_RdTscInst(X86_RdTscInst &i)
{
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Init::VisitLoadInst(LoadInst &i)
{
  analysis_.Mark(i, Infer(i.GetType()));
}

// -----------------------------------------------------------------------------
void Init::VisitBitCountInst(BitCountInst &i)
{
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Init::VisitNegInst(NegInst &i)
{
  // TODO: propagate even/odd.
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Init::VisitRotateInst(RotateInst &i)
{
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Init::VisitSyscallInst(SyscallInst &i)
{
  analysis_.Mark(i, TaggedType::PtrInt());
}

// -----------------------------------------------------------------------------
void Init::VisitCloneInst(CloneInst &i)
{
  analysis_.Mark(i, TaggedType::PtrInt());
}

// -----------------------------------------------------------------------------
void Init::VisitLandingPadInst(LandingPadInst &pad)
{
  if (target_) {
    switch (target_->GetKind()) {
      case Target::Kind::X86: {
        analysis_.Mark(pad.GetSubValue(0), TaggedType::Ptr());
        analysis_.Mark(pad.GetSubValue(1), TaggedType::Young());
        for (unsigned i = 2, n = pad.GetNumRets(); i < n; ++i) {
          analysis_.Mark(pad.GetSubValue(i), Infer(pad.GetType(i)));
        }
        return;
      }
      case Target::Kind::PPC: {
        llvm_unreachable("not implemented");
      }
      case Target::Kind::AARCH64: {
        llvm_unreachable("not implemented");
      }
      case Target::Kind::RISCV: {
        llvm_unreachable("not implemented");
      }
    }
    llvm_unreachable("invalid target kind");
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
TaggedType Init::Infer(Type ty)
{
  switch (ty) {
    case Type::V64: {
      return TaggedType::Val();
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128: {
      if (target_->GetPointerType() == ty) {
        return TaggedType::PtrInt();
      } else {
        return TaggedType::Int();
      }
    }
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      return TaggedType::Int();
    }
  }
  llvm_unreachable("invalid type");
}
