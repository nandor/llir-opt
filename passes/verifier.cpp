// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
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
  return "Move Elimination";
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
void VerifierPass::Verify(Inst &i)
{
  auto GetType = [this, &i](Inst *inst) {
    if (inst->GetNumRets() == 0) {
      Error(i, "missing type");
    }
    return inst->GetType(0);
  };
  auto CheckType = [this, &i, &GetType](Inst *inst, Type type) {
    if (GetType(inst) != type) {
      Error(i, "callee not a pointer");
    }
  };

  switch (i.GetKind()) {
    case Inst::Kind::CALL: {
      auto &call = static_cast<CallInst &>(i);
      CheckType(call.GetCallee(), GetPointerType());
      // TODO: check arguments for direct callees.
      return;
    }
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE:
    case Inst::Kind::TINVOKE: {
      auto &call = static_cast<CallSite<TerminatorInst> &>(i);
      CheckType(call.GetCallee(), GetPointerType());
      // TODO: check arguments for direct callees.
      return;
    }
    case Inst::Kind::SYSCALL: {
      return;
    }

    case Inst::Kind::ARG: {
      auto &arg = static_cast<ArgInst &>(i);
      unsigned idx = arg.GetIdx();
      const auto &params = i.getParent()->getParent()->params();
      if (idx > params.size()) {
        Error(i, "argument out of range");
      }
      if (params[idx] != arg.GetType()) {
        Error(i, "argument type mismatch");
      }
      return;
    }

    case Inst::Kind::JI: {
      CheckType(static_cast<JumpIndirectInst &>(i).GetTarget(), GetPointerType());
      return;
    }
    case Inst::Kind::LD: {
      CheckType(static_cast<LoadInst &>(i).GetAddr(), GetPointerType());
      return;
    }
    case Inst::Kind::ST: {
      CheckType(static_cast<StoreInst &>(i).GetAddr(), GetPointerType());
      return;
    }
    case Inst::Kind::FNSTCW: {
      CheckType(static_cast<FNStCwInst &>(i).GetAddr(), GetPointerType());
      return;
    }
    case Inst::Kind::FLDCW: {
      CheckType(static_cast<FLdCwInst &>(i).GetAddr(), GetPointerType());
      return;
    }
    case Inst::Kind::VASTART:{
      CheckType(static_cast<VAStartInst &>(i).GetVAList(), GetPointerType());
      return;
    }

    case Inst::Kind::XCHG: {
      auto &xchg = static_cast<XchgInst &>(i);
      CheckType(xchg.GetAddr(), GetPointerType());
      if (GetType(xchg.GetVal()) != xchg.GetType()) {
        Error(i, "invalid exchange");
      }
      return;
    }
    case Inst::Kind::CMPXCHG: {
      auto &cmpXchg = static_cast<CmpXchgInst &>(i);
      CheckType(cmpXchg.GetAddr(), GetPointerType());
      if (GetType(cmpXchg.GetVal()) != cmpXchg.GetType()) {
        Error(i, "invalid exchange");
      }
      if (GetType(cmpXchg.GetRef()) != cmpXchg.GetType()) {
        Error(i, "invalid exchange");
      }
      return;
    }
    case Inst::Kind::SELECT: {
      auto &sel = static_cast<SelectInst &>(i);
      if (GetType(sel.GetTrue()) != sel.GetType()) {
        Error(i, "mismatched true branch");
      }
      if (GetType(sel.GetFalse()) != sel.GetType()) {
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
      Type type = phi.GetType();
      for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
        Value *value = phi.GetValue(i);
        switch (value->GetKind()) {
          case Value::Kind::INST: {
            if (GetType(static_cast<Inst *>(value)) != type) {
              Error(phi, "phi instruction argument invalid");
            }
            continue;
          }
          case Value::Kind::GLOBAL: {
            if (phi.GetType() != GetPointerType()) {
              Error(phi, "phi must be of pointer type");
            }
            continue;
          }
          case Value::Kind::EXPR: {
            switch (static_cast<Expr *>(value)->GetKind()) {
              case Expr::Kind::SYMBOL_OFFSET: {
                if (phi.GetType() != GetPointerType()) {
                  Error(phi, "phi must be of pointer type");
                }
                continue;
              }
            }
            llvm_unreachable("invalid expression kind");
          }
          case Value::Kind::CONST: {
            switch (static_cast<Constant *>(value)->GetKind()) {
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
        case ConstantReg::Kind::RAX:
        case ConstantReg::Kind::RBX:
        case ConstantReg::Kind::RCX:
        case ConstantReg::Kind::RDX:
        case ConstantReg::Kind::RSI:
        case ConstantReg::Kind::RDI:
        case ConstantReg::Kind::RSP:
        case ConstantReg::Kind::RBP:
        case ConstantReg::Kind::R8:
        case ConstantReg::Kind::R9:
        case ConstantReg::Kind::R10:
        case ConstantReg::Kind::R11:
        case ConstantReg::Kind::R12:
        case ConstantReg::Kind::R13:
        case ConstantReg::Kind::R14:
        case ConstantReg::Kind::R15: {
          if (GetType(set.GetValue()) != Type::I64) {
            Error(i, "64-bit integer expected");
          }
          return;
        }
        case ConstantReg::Kind::FS:
        case ConstantReg::Kind::RET_ADDR:
        case ConstantReg::Kind::FRAME_ADDR:
        case ConstantReg::Kind::PC: {
          if (GetType(set.GetValue()) != GetPointerType()) {
            Error(i, "pointer type expected");
          }
          return;
        }
      }
      llvm_unreachable("invalid register kind");
    }

    case Inst::Kind::ALLOCA:
    case Inst::Kind::FRAME: {
      if (GetType(&i) != GetPointerType()) {
        Error(i, "pointer type expected");
      }
      return;
    }

    case Inst::Kind::MOV: {
      auto &mi = static_cast<MovInst &>(i);
      Value *value = mi.GetArg();
      switch (value->GetKind()) {
        case Value::Kind::INST: {
          return;
        }
        case Value::Kind::GLOBAL: {
          if (mi.GetType() != GetPointerType()) {
            Error(i, "global move not pointer sized");
          }
          return;
        }
        case Value::Kind::EXPR: {
          switch (static_cast<Expr *>(value)->GetKind()) {
            case Expr::Kind::SYMBOL_OFFSET: {
              if (mi.GetType() != GetPointerType()) {
                Error(i, "pointer type expected");
              }
              return;
            }
          }
          llvm_unreachable("invalid expression kind");
        }
        case Value::Kind::CONST: {
          switch (static_cast<Constant *>(value)->GetKind()) {
            case Constant::Kind::INT: {
              return;
            }
            case Constant::Kind::FLOAT: {
              return;
            }
            case Constant::Kind::REG: {
              auto *reg = static_cast<ConstantReg *>(value);
              switch (reg->GetValue()) {
                case ConstantReg::Kind::RAX:
                case ConstantReg::Kind::RBX:
                case ConstantReg::Kind::RCX:
                case ConstantReg::Kind::RDX:
                case ConstantReg::Kind::RSI:
                case ConstantReg::Kind::RDI:
                case ConstantReg::Kind::RSP:
                case ConstantReg::Kind::RBP:
                case ConstantReg::Kind::R8:
                case ConstantReg::Kind::R9:
                case ConstantReg::Kind::R10:
                case ConstantReg::Kind::R11:
                case ConstantReg::Kind::R12:
                case ConstantReg::Kind::R13:
                case ConstantReg::Kind::R14:
                case ConstantReg::Kind::R15: {
                  if (mi.GetType() != Type::I64) {
                    Error(i, "64-bit integer expected");
                  }
                  return;
                }
                case ConstantReg::Kind::FS:
                case ConstantReg::Kind::RET_ADDR:
                case ConstantReg::Kind::FRAME_ADDR:
                case ConstantReg::Kind::PC: {
                  if (mi.GetType() != GetPointerType()) {
                    Error(i, "pointer type expected");
                  }
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
    case Inst::Kind::CLZ: {
      // Argument must match type.
      auto &ui = static_cast<UnaryInst &>(i);
      Type type = ui.GetType();

      Inst *lhs = ui.GetArg();
      if (lhs->GetNumRets() == 0) {
        Error(i, "missing argument type");
      }
      if (lhs->GetType(0) != type) {
        Error(i, "invalid argument type");
      }
      return;
    }

    case Inst::Kind::ADD:
    case Inst::Kind::SUB:
    case Inst::Kind::AND:
    case Inst::Kind::UDIV:
    case Inst::Kind::SDIV:
    case Inst::Kind::UREM:
    case Inst::Kind::SREM:
    case Inst::Kind::MUL:
    case Inst::Kind::OR:
    case Inst::Kind::XOR:
    case Inst::Kind::POW:
    case Inst::Kind::COPYSIGN: {
      // All types must match.
      auto &bi = static_cast<BinaryInst &>(i);
      Type type = bi.GetType();
      CheckType(bi.GetLHS(), type);
      CheckType(bi.GetRHS(), type);
      return;
    }

    case Inst::Kind::CMP: {
      // Arguments must be of identical type.
      auto &bi = static_cast<CmpInst &>(i);
      if (GetType(bi.GetLHS()) != GetType(bi.GetRHS())) {
        Error(i, "invalid arguments");
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
      CheckType(bi.GetLHS(), type);
      if (!IsIntegerType(GetType(bi.GetRHS()))) {
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
      if (GetType(bi.GetLHS()) != GetType(bi.GetRHS())) {
        Error(i, "invalid argument types");
      }
      return;
    }
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
void VerifierPass::Error(Inst &i, const char *msg)
{
  const Block *block = i.getParent();
  const Func *func = block->getParent();
  std::ostringstream os;
  os << "[" << func->GetName() << ":" << block->GetName() << "] " << msg;
  llvm::report_fatal_error(os.str().c_str());
}
