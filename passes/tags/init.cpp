// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/object.h"
#include "core/data.h"
#include "passes/tags/init.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
void Init::VisitArgInst(const ArgInst &i)
{
  auto &func = *i.getParent()->getParent();
  switch (func.GetCallingConv()) {
    case CallingConv::C:
    case CallingConv::SETJMP:
    case CallingConv::XEN:
    case CallingConv::INTR:
    case CallingConv::MULTIBOOT:
    case CallingConv::WIN64: {
      if (func.HasAddressTaken()) {
        analysis_.Mark(i, TaggedType::Any());
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
                if (func.HasAddressTaken()) {
                  switch (auto ty = i.GetType()) {
                    case Type::V64: {
                      analysis_.Mark(i, TaggedType::Val());
                      return;
                    }
                    case Type::I8:
                    case Type::I16:
                    case Type::I32:
                    case Type::I64:
                    case Type::I128: {
                      if (target_->GetPointerType() == ty) {
                        analysis_.Mark(i, TaggedType::IntOrPtr());
                      } else {
                        analysis_.Mark(i, TaggedType::Int());
                      }
                      return;
                    }
                    case Type::F32:
                    case Type::F64:
                    case Type::F80:
                    case Type::F128: {
                      analysis_.Mark(i, TaggedType::Int());
                      return;
                    }
                  }
                  llvm_unreachable("invalid type");
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
void Init::VisitMovInst(const MovInst &i)
{
  auto global = [this, &i](ConstRef<Global> g)
  {
    switch (g->GetKind()) {
      case Global::Kind::EXTERN: {
        analysis_.Mark(i, TaggedType::Heap());
        return;
      }
      case Global::Kind::FUNC:
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
          if (v.isNullValue()) {
            analysis_.Mark(i, TaggedType::Zero());
          } else if (v.isOneValue()) {
            analysis_.Mark(i, TaggedType::One());
          } else if (v[0]) {
            analysis_.Mark(i, TaggedType::Odd());
          } else {
            analysis_.Mark(i, TaggedType::Even());
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
void Init::VisitFrameInst(const FrameInst &i)
{
  analysis_.Mark(i, TaggedType::Ptr());
}

// -----------------------------------------------------------------------------
void Init::VisitAllocaInst(const AllocaInst &i)
{
  analysis_.Mark(i, TaggedType::Ptr());
}

// -----------------------------------------------------------------------------
void Init::VisitGetInst(const GetInst &i)
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
    case Register::X86_CR4:
    case Register::X86_DS:
    case Register::X86_ES:
    case Register::X86_SS:
    case Register::X86_FS:
    case Register::X86_GS:
    case Register::X86_CS:
    case Register::AARCH64_FPSR:
    case Register::AARCH64_FPCR:
    case Register::AARCH64_CNTVCT:
    case Register::AARCH64_CNTFRQ:
    case Register::AARCH64_FAR:
    case Register::AARCH64_VBAR:
    case Register::RISCV_FFLAGS:
    case Register::RISCV_FRM:
    case Register::RISCV_FCSR:
    case Register::PPC_FPSCR: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid register kind");
}

// -----------------------------------------------------------------------------
void Init::VisitUndefInst(const UndefInst &i)
{
  analysis_.Mark(i, TaggedType::Undef());
}

// -----------------------------------------------------------------------------
void Init::VisitX86_RdTscInst(const X86_RdTscInst &i)
{
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Init::VisitCmpInst(const CmpInst &i)
{
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Init::VisitLoadInst(const LoadInst &i)
{
  switch (auto ty = i.GetType()) {
    case Type::V64: {
      analysis_.Mark(i, TaggedType::Val());
      return;
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128: {
      if (target_->GetPointerType() == ty) {
        analysis_.Mark(i, TaggedType::IntOrPtr());
      } else {
        analysis_.Mark(i, TaggedType::Int());
      }
      return;
    }
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
void Init::VisitBitCountInst(const BitCountInst &i)
{
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Init::VisitNegInst(const NegInst &i)
{
  // TODO: propagate even/odd.
  analysis_.Mark(i, TaggedType::Int());
}


// -----------------------------------------------------------------------------
void Init::VisitRotateInst(const RotateInst &i)
{
  analysis_.Mark(i, TaggedType::Int());
}
