// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Support/raw_ostream.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/printer.h"
#include "passes/verifier.h"



// -----------------------------------------------------------------------------
const char *VerifierPass::kPassID = "verifier";

// -----------------------------------------------------------------------------
void VerifierPass::Run(Prog *prog)
{
  for (Func &func : *prog) {
    Verify(func);
  }
}

// -----------------------------------------------------------------------------
const char *VerifierPass::GetPassName() const
{
  return "Verifier";
}

// -----------------------------------------------------------------------------
void VerifierPass::Verify(Func &func)
{
  for (Block &block : func) {
    for (Inst &inst : block) {
      Verify(inst);
    }
  }
}

// -----------------------------------------------------------------------------
static bool Compatible(Type vt, Type type)
{
  if (vt == type) {
    return true;
  }
  if (type == Type::V64 || type == Type::I64) {
    return vt == Type::I64 || vt == Type::V64;
  }
  return false;
}

// -----------------------------------------------------------------------------
void VerifierPass::Verify(Inst &i)
{
  switch (i.GetKind()) {
    case Inst::Kind::CALL:
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE: {
      auto &call = static_cast<CallSite &>(i);
      CheckPointer(i, call.GetCallee());
      // TODO: check arguments for direct callees.
      return;
    }
    case Inst::Kind::SYSCALL: {
      return;
    }
    case Inst::Kind::CLONE: {
      return;
    }
    case Inst::Kind::AARCH64_DMB: {
      return;
    }

    case Inst::Kind::ARG: {
      auto &arg = static_cast<ArgInst &>(i);
      unsigned idx = arg.GetIdx();
      const auto &params = i.getParent()->getParent()->params();
      if (idx >= params.size()) {
        Error(i, "argument out of range");
      }
      if (params[idx] != arg.GetType()) {
        Error(i, "argument type mismatch");
      }
      return;
    }

    case Inst::Kind::RAISE: {
      auto &inst = static_cast<RaiseInst &>(i);
      CheckPointer(i, inst.GetTarget());
      CheckPointer(i, inst.GetStack());
      return;
    }
    case Inst::Kind::LD: {
      CheckPointer(i, static_cast<LoadInst &>(i).GetAddr());
      return;
    }
    case Inst::Kind::AARCH64_LL: {
      CheckPointer(i, static_cast<StoreInst &>(i).GetAddr());
      return;
    }
    case Inst::Kind::ST: {
      CheckPointer(i, static_cast<AArch64_LL &>(i).GetAddr());
      return;
    }
    case Inst::Kind::AARCH64_SC: {
      CheckPointer(i, static_cast<AArch64_SC &>(i).GetAddr());
      return;
    }
    case Inst::Kind::X86_FNSTCW:
    case Inst::Kind::X86_FNSTSW:
    case Inst::Kind::X86_FNSTENV:
    case Inst::Kind::X86_FLDCW:
    case Inst::Kind::X86_FLDENV:
    case Inst::Kind::X86_LDMXCSR:
    case Inst::Kind::X86_STMXCSR: {
      auto &inst = static_cast<X86_FPUControlInst &>(i);
      CheckPointer(i, inst.GetAddr());
      return;
    }
    case Inst::Kind::VASTART:{
      CheckPointer(i, static_cast<VAStartInst &>(i).GetVAList());
      return;
    }

    case Inst::Kind::X86_XCHG: {
      auto &xchg = static_cast<X86_XchgInst &>(i);
      CheckPointer(i, xchg.GetAddr());
      if (xchg.GetVal().GetType() != xchg.GetType()) {
        Error(i, "invalid exchange");
      }
      return;
    }
    case Inst::Kind::X86_CMPXCHG: {
      auto &cmpXchg = static_cast<X86_CmpXchgInst &>(i);
      CheckPointer(i, cmpXchg.GetAddr());
      if (cmpXchg.GetVal().GetType() != cmpXchg.GetType()) {
        Error(i, "invalid exchange");
      }
      if (cmpXchg.GetRef().GetType() != cmpXchg.GetType()) {
        Error(i, "invalid exchange");
      }
      return;
    }
    case Inst::Kind::SELECT: {
      auto &sel = static_cast<SelectInst &>(i);
      if (!Compatible(sel.GetTrue().GetType(), sel.GetType())) {
        Error(i, "mismatched true branch");
      }
      if (!Compatible(sel.GetFalse().GetType(), sel.GetType())) {
        Error(i, "mismatched false branches");
      }
      return;
    }

    case Inst::Kind::SEXT:
    case Inst::Kind::ZEXT:
    case Inst::Kind::FEXT:
    case Inst::Kind::XEXT:
    case Inst::Kind::TRUNC: {
      return;
    }

    case Inst::Kind::PHI: {
      auto &phi = static_cast<PhiInst &>(i);
      for (Block *predBB : phi.getParent()->predecessors()) {
        if (!phi.HasValue(predBB)) {
          Error(phi, "missing predecessor to phi: " + predBB->getName());
        }
      }
      Type type = phi.GetType();
      for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
        Ref<Value> value = phi.GetValue(i);
        switch (value->GetKind()) {
          case Value::Kind::INST: {
            auto vt = cast<Inst>(value).GetType();
            if (!Compatible(vt, type)) {
              Error(phi, "phi instruction argument invalid");
            }
            continue;
          }
          case Value::Kind::GLOBAL: {
            CheckPointer(phi, &phi, "phi must be of pointer type");
            continue;
          }
          case Value::Kind::EXPR: {
            const Expr &e = *cast<Expr>(value);
            switch (e.GetKind()) {
              case Expr::Kind::SYMBOL_OFFSET: {
                CheckPointer(phi, &phi, "phi must be of pointer type");
                continue;
              }
            }
            llvm_unreachable("invalid expression kind");
          }
          case Value::Kind::CONST: {
            const Constant &c = *cast<Constant>(value);
            switch (c.GetKind()) {
              case Constant::Kind::INT: {
                continue;
              }
              case Constant::Kind::FLOAT: {
                return;
              }
              case Constant::Kind::REG: {
                llvm_unreachable("invalid incoming register to phi");
              }
            }
            llvm_unreachable("invalid constant kind");
          }
        }
        llvm_unreachable("invalid value kind");
      }
      return;
    }

    case Inst::Kind::SET: {
      auto &set = static_cast<SetInst &>(i);
      switch (set.GetReg()->GetValue()) {
        case ConstantReg::Kind::SP:
        case ConstantReg::Kind::FS:
        case ConstantReg::Kind::RET_ADDR:
        case ConstantReg::Kind::FRAME_ADDR:
        case ConstantReg::Kind::AARCH64_FPSR:
        case ConstantReg::Kind::AARCH64_FPCR: {
          CheckPointer(i, set.GetValue(), "set expects a pointer");
          return;
        }
      }
      llvm_unreachable("invalid register kind");
    }

    case Inst::Kind::ALLOCA:
    case Inst::Kind::FRAME: {
      if (i.GetType(0) != GetPointerType()) {
        Error(i, "pointer type expected");
      }
      return;
    }

    case Inst::Kind::MOV: {
      auto &mi = static_cast<MovInst &>(i);
      Ref<Value> value = mi.GetArg();
      switch (value->GetKind()) {
        case Value::Kind::INST: {
          return;
        }
        case Value::Kind::GLOBAL: {
          CheckPointer(i, &mi, "global move not pointer sized");
          return;
        }
        case Value::Kind::EXPR: {
          const Expr &e = *cast<Expr>(value);
          switch (e.GetKind()) {
            case Expr::Kind::SYMBOL_OFFSET: {
              CheckPointer(i, &mi, "expression must be a pointer");
              return;
            }
          }
          llvm_unreachable("invalid expression kind");
        }
        case Value::Kind::CONST: {
          const Constant &c = *cast<Constant>(value);
          switch (c.GetKind()) {
            case Constant::Kind::INT: {
              return;
            }
            case Constant::Kind::FLOAT: {
              return;
            }
            case Constant::Kind::REG: {
              auto &reg = static_cast<const ConstantReg &>(c);
              switch (reg.GetValue()) {
                case ConstantReg::Kind::SP:
                case ConstantReg::Kind::FS:
                case ConstantReg::Kind::RET_ADDR:
                case ConstantReg::Kind::FRAME_ADDR:
                case ConstantReg::Kind::AARCH64_FPSR:
                case ConstantReg::Kind::AARCH64_FPCR: {
                  CheckPointer(i, &mi, "registers return pointers");
                  return;
                }
              }
              llvm_unreachable("invalid register kind");
            }
          }
          llvm_unreachable("invalid constant kind");
        }
      }
      llvm_unreachable("invalid value kind");
    }

    case Inst::Kind::RDTSC:
    case Inst::Kind::X86_FNCLEX:
    case Inst::Kind::UNDEF:
    case Inst::Kind::SWITCH:
    case Inst::Kind::JCC:
    case Inst::Kind::JMP:
    case Inst::Kind::TRAP:
    case Inst::Kind::RET: {
      return;
    }

    case Inst::Kind::ABS:
    case Inst::Kind::NEG:
    case Inst::Kind::SQRT:
    case Inst::Kind::SIN:
    case Inst::Kind::COS:
    case Inst::Kind::EXP:
    case Inst::Kind::EXP2:
    case Inst::Kind::LOG:
    case Inst::Kind::LOG2:
    case Inst::Kind::LOG10:
    case Inst::Kind::FCEIL:
    case Inst::Kind::FFLOOR:
    case Inst::Kind::POPCNT:
    case Inst::Kind::BSWAP:
    case Inst::Kind::CLZ:
    case Inst::Kind::CTZ: {
      // Argument must match type.
      auto &ui = static_cast<UnaryInst &>(i);
      if (ui.GetArg().GetType() != ui.GetType()) {
        Error(i, "invalid argument type");
      }
      return;
    }

    case Inst::Kind::ADD:
    case Inst::Kind::SUB:
    case Inst::Kind::AND:
    case Inst::Kind::OR:
    case Inst::Kind::XOR: {
      // TODO: check v64 operations.
      return;
    }
    case Inst::Kind::UDIV:
    case Inst::Kind::SDIV:
    case Inst::Kind::UREM:
    case Inst::Kind::SREM:
    case Inst::Kind::MUL:
    case Inst::Kind::POW:
    case Inst::Kind::COPYSIGN: {
      // All types must match.
      auto &bi = static_cast<BinaryInst &>(i);
      Type type = bi.GetType();
      CheckType(i, bi.GetLHS(), type);
      CheckType(i, bi.GetRHS(), type);
      return;
    }

    case Inst::Kind::CMP: {
      // Arguments must be of identical type.
      auto &bi = static_cast<CmpInst &>(i);
      auto lt = bi.GetLHS().GetType();
      auto rt = bi.GetRHS().GetType();
      bool lptr = lt == Type::V64 && rt == Type::I64;
      bool rptr = rt == Type::V64 && lt == Type::I64;
      if (lt != rt && !lptr && !rptr) {
        Error(i, "invalid arguments to comparison");
      }
      return;
    }


    case Inst::Kind::ROTL:
    case Inst::Kind::ROTR:
    case Inst::Kind::SLL:
    case Inst::Kind::SRA:
    case Inst::Kind::SRL: {
      // LHS must be integral and match, rhs also integral.
      auto &bi = static_cast<BinaryInst &>(i);
      Type type = bi.GetType();
      if (!IsIntegerType(type)) {
        Error(i, "integral type expected");
      }
      CheckType(i, bi.GetLHS(), type);
      if (!IsIntegerType(bi.GetRHS().GetType())) {
        Error(i, "integral type expected");
      }
      return;
    }

    case Inst::Kind::UADDO:
    case Inst::Kind::UMULO:
    case Inst::Kind::USUBO:
    case Inst::Kind::SADDO:
    case Inst::Kind::SMULO:
    case Inst::Kind::SSUBO: {
      // LHS must match RHS, return type integral.
      auto &bi = static_cast<OverflowInst &>(i);
      Type type = bi.GetType();
      if (!IsIntegerType(type)) {
        Error(i, "integral type expected");
      }
      if (bi.GetLHS().GetType() != bi.GetRHS().GetType()) {
        Error(i, "invalid argument types");
      }
      return;
    }
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
void VerifierPass::CheckPointer(const Inst &i, Ref<Inst> ref, const char *msg)
{
  if (ref.GetType() != Type::I64 && ref.GetType() != Type::V64) {
    Error(i, msg);
  }
};

// -----------------------------------------------------------------------------
void VerifierPass::CheckInteger(const Inst &i, Ref<Inst> ref, const char *msg)
{
  if (!IsIntegerType(ref.GetType())) {
    Error(i, msg);
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::CheckType(const Inst &i, Ref<Inst> ref, Type type)
{
  auto it = ref.GetType();
  if (type == Type::I64 && it == Type::V64) {
    return;
  }
  if (type == Type::V64 && it == Type::I64) {
    return;
  }
  if (it != type) {
    Error(i, "invalid type");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::Error(const Inst &i, llvm::Twine msg)
{
  const Block *block = i.getParent();
  const Func *func = block->getParent();
  std::string buffer;
  llvm::raw_string_ostream os(buffer);
  Printer p(os);
  os << "[" << func->GetName() << ":" << block->GetName() << "] " << msg.str();
  os << "\n\n";
  p.Print(*i.getParent()->getParent());
  p.Print(i);
  os << "\n";
  llvm::report_fatal_error(os.str().c_str());
}
