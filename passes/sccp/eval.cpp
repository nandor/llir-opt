// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/APSInt.h>

#include "core/atom.h"
#include "core/object.h"
#include "core/data.h"
#include "core/cond.h"
#include "core/func.h"
#include "passes/sccp/eval.h"


// -----------------------------------------------------------------------------
const llvm::fltSemantics &getSemantics(Type ty)
{
  switch (ty) {
    default: llvm_unreachable("not a float type");
    case Type::F32:  return llvm::APFloat::IEEEsingle();
    case Type::F64:  return llvm::APFloat::IEEEdouble();
    case Type::F80:  return llvm::APFloat::x87DoubleExtended();
    case Type::F128: return llvm::APFloat::IEEEquad();
  }
}

// -----------------------------------------------------------------------------
static Lattice MakeBoolean(bool value, Type ty)
{
  switch (ty) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128:
      return Lattice::CreateInteger(APInt(GetSize(ty) * 8, value, true));
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::V64:
    case Type::F128:
      llvm_unreachable("invalid comparison");
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Extend(const Lattice &arg, Type ty)
{
  switch (arg.GetKind()) {
    case Lattice::Kind::UNKNOWN:
    case Lattice::Kind::OVERDEFINED:
    case Lattice::Kind::UNDEFINED: {
      return arg;
    }
    case Lattice::Kind::INT: {
      const auto &i = arg.GetInt();
      switch (ty) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::I128:
        case Type::V64: {
          return Lattice::CreateInteger(i.sextOrTrunc(GetSize(ty) * 8));
        }
        case Type::F32:
        case Type::F64:
        case Type::F128: {
          // TODO: implement this
          return Lattice::Overdefined();
        }
        case Type::F80: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Lattice::Kind::FLOAT: {
      const auto &f = arg.GetFloat();
      switch (ty) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::I128: {
          llvm_unreachable("not implemented");
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          bool lossy;
          APFloat r(f);
          r.convert(getSemantics(ty), APFloat::rmNearestTiesToEven, &lossy);
          return Lattice::CreateFloat(r);
        }
        case Type::V64: {
          // Cannot extend integer to pointer.
          return Lattice::Overdefined();
        }
      }
      llvm_unreachable("invalid type");
    }
    case Lattice::Kind::MASK: {
      switch (ty) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::V64:
        case Type::I128: {
          auto k = arg.GetKnown();
          auto v = arg.GetValue();
          auto b = GetBitWidth(ty);
          assert(k.getBitWidth() == v.getBitWidth() && "invalid width");
          if (k.getBitWidth() == b) {
            return arg;
          } else {
            return Lattice::CreateMask(k.zextOrTrunc(b), v.zextOrTrunc(b));
          }
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid type");
    }
    case Lattice::Kind::FLOAT_ZERO: {
      switch (ty) {
        case Type::V64:
        case Type::I64:
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I128: {
          llvm_unreachable("not implemented");
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          return arg;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case Lattice::Kind::FRAME:
    case Lattice::Kind::GLOBAL:
    case Lattice::Kind::POINTER:
    case Lattice::Kind::RANGE: {
      switch (ty) {
        case Type::V64:
        case Type::I64: {
          return arg;
        }
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I128:
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
  }
  llvm_unreachable("invalid value kind");
}


// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(BitCastInst *inst, Lattice &arg)
{
  auto ty = inst->GetType();
  switch (arg.GetKind()) {
    case Lattice::Kind::UNKNOWN:
    case Lattice::Kind::OVERDEFINED:
    case Lattice::Kind::UNDEFINED:
    case Lattice::Kind::POINTER: {
      return arg;
    }
    case Lattice::Kind::FLOAT_ZERO: {
      return Lattice::Overdefined();
    }
    case Lattice::Kind::INT: {
      switch (ty) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::V64:
        case Type::I128: {
          APInt i = arg.GetInt();
          return Lattice::CreateInteger(i.sextOrTrunc(GetSize(ty) * 8));
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          // TODO: implement the bit cast
          return Lattice::Overdefined();
        }
      }
      llvm_unreachable("invalid type");
    }
    case Lattice::Kind::MASK: {
      switch (ty) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::V64:
        case Type::I128: {
          unsigned bits = GetBitWidth(ty);
          return Lattice::CreateMask(
              arg.GetKnown().zextOrTrunc(bits),
              arg.GetValue().zextOrTrunc(bits)
          );
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          return Lattice::Overdefined();
        }
      }
      llvm_unreachable("invalid type");
    }
    case Lattice::Kind::FLOAT: {
      switch (ty) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::V64:
        case Type::I128: {
          APInt i = arg.GetFloat().bitcastToAPInt();
          return Lattice::CreateInteger(i.sextOrTrunc(GetSize(ty) * 8));
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          // TODO: implement the bit cast.
          return Lattice::Overdefined();
        }
      }
      llvm_unreachable("invalid type");
    }
    case Lattice::Kind::FRAME:
    case Lattice::Kind::GLOBAL:
    case Lattice::Kind::RANGE: {
      switch (ty) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I128: {
          llvm_unreachable("not implemented");
        }
        case Type::V64:
        case Type::I64: {
          return arg;
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          llvm_unreachable("not implemented");
        }
      }
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(UnaryInst *inst, Lattice &arg)
{
  assert(!arg.IsUnknown() && "invalid argument");
  if (arg.IsOverdefined()) {
    return Lattice::Overdefined();
  }
  if (arg.IsUndefined()) {
    return Lattice::Undefined();
  }

  switch (inst->GetKind()) {
    default: llvm_unreachable("not a unary instruction");

    case Inst::Kind::ABS:        return Eval(static_cast<AbsInst *>(inst),      arg);
    case Inst::Kind::NEG:        return Eval(static_cast<NegInst *>(inst),      arg);
    case Inst::Kind::SQRT:       return Eval(static_cast<SqrtInst *>(inst),     arg);
    case Inst::Kind::SIN:        return Eval(static_cast<SinInst *>(inst),      arg);
    case Inst::Kind::COS:        return Eval(static_cast<CosInst *>(inst),      arg);
    case Inst::Kind::S_EXT:      return Eval(static_cast<SExtInst *>(inst),     arg);
    case Inst::Kind::Z_EXT:      return Eval(static_cast<ZExtInst *>(inst),     arg);
    case Inst::Kind::F_EXT:      return Eval(static_cast<FExtInst *>(inst),     arg);
    case Inst::Kind::X_EXT:      return Eval(static_cast<XExtInst *>(inst),     arg);
    case Inst::Kind::TRUNC:      return Eval(static_cast<TruncInst *>(inst),    arg);
    case Inst::Kind::EXP:        return Eval(static_cast<ExpInst *>(inst),      arg);
    case Inst::Kind::EXP2:       return Eval(static_cast<Exp2Inst *>(inst),     arg);
    case Inst::Kind::LOG:        return Eval(static_cast<LogInst *>(inst),      arg);
    case Inst::Kind::LOG2:       return Eval(static_cast<Log2Inst *>(inst),     arg);
    case Inst::Kind::LOG10:      return Eval(static_cast<Log10Inst *>(inst),    arg);
    case Inst::Kind::F_CEIL:     return Eval(static_cast<FCeilInst *>(inst),    arg);
    case Inst::Kind::F_FLOOR:    return Eval(static_cast<FFloorInst *>(inst),   arg);
    case Inst::Kind::POP_COUNT:  return Eval(static_cast<PopCountInst *>(inst), arg);
    case Inst::Kind::CLZ:        return Eval(static_cast<ClzInst *>(inst),      arg);
    case Inst::Kind::CTZ:        return Eval(static_cast<CtzInst *>(inst),      arg);
    case Inst::Kind::BYTE_SWAP:  return Eval(static_cast<ByteSwapInst *>(inst), arg);
    case Inst::Kind::BIT_CAST:   return Eval(static_cast<BitCastInst *>(inst),  arg);
  }
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(BinaryInst *inst, Lattice &l, Lattice &r)
{
  assert(!l.IsUnknown() && "invalid l");
  assert(!r.IsUnknown() && "invalid r");
  if (l.IsUndefined() || r.IsUndefined()) {
    return Lattice::Undefined();
  }

  const auto ty = inst->GetType();
  switch (inst->GetKind()) {
    default: llvm_unreachable("not a binary instruction");

    case Inst::Kind::SLL:       return Eval(Bitwise::SLL,  ty, l, r);
    case Inst::Kind::SRA:       return Eval(Bitwise::SRA,  ty, l, r);
    case Inst::Kind::SRL:       return Eval(Bitwise::SRL,  ty, l, r);
    case Inst::Kind::ROTL:      return Eval(Bitwise::ROTL, ty, l, r);
    case Inst::Kind::ROTR:      return Eval(Bitwise::ROTR, ty, l, r);
    case Inst::Kind::ADD:       return Eval(static_cast<AddInst *>(inst),      l, r);
    case Inst::Kind::SUB:       return Eval(static_cast<SubInst *>(inst),      l, r);
    case Inst::Kind::AND:       return Eval(static_cast<AndInst *>(inst),      l, r);
    case Inst::Kind::OR:        return Eval(static_cast<OrInst *>(inst),       l, r);
    case Inst::Kind::XOR:       return Eval(static_cast<XorInst *>(inst),      l, r);
    case Inst::Kind::U_DIV:     return Eval(static_cast<UDivInst *>(inst),     l, r);
    case Inst::Kind::S_DIV:     return Eval(static_cast<SDivInst *>(inst),     l, r);
    case Inst::Kind::U_REM:     return Eval(static_cast<URemInst *>(inst),     l, r);
    case Inst::Kind::S_REM:     return Eval(static_cast<SRemInst *>(inst),     l, r);
    case Inst::Kind::MUL:       return Eval(static_cast<MulInst *>(inst),      l, r);
    case Inst::Kind::MUL_H_S:   return Eval(static_cast<MulHSInst *>(inst),      l, r);
    case Inst::Kind::MUL_H_U:   return Eval(static_cast<MulHUInst *>(inst),      l, r);
    case Inst::Kind::POW:       return Eval(static_cast<PowInst *>(inst),      l, r);
    case Inst::Kind::COPY_SIGN: return Eval(static_cast<CopySignInst *>(inst), l, r);
    case Inst::Kind::O_U_ADD:   return Eval(static_cast<OUAddInst *>(inst),    l, r);
    case Inst::Kind::O_U_MUL:   return Eval(static_cast<OUMulInst *>(inst),    l, r);
    case Inst::Kind::O_U_SUB:   return Eval(static_cast<OUSubInst *>(inst),    l, r);
    case Inst::Kind::O_S_ADD:   return Eval(static_cast<OSAddInst *>(inst),    l, r);
    case Inst::Kind::O_S_MUL:   return Eval(static_cast<OSMulInst *>(inst),    l, r);
    case Inst::Kind::O_S_SUB:   return Eval(static_cast<OSSubInst *>(inst),    l, r);
  }
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(AbsInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(NegInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(-*i);
      }
      llvm_unreachable("cannot negate non-integer");
    }
    case Type::F64: case Type::F32: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SqrtInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      llvm_unreachable("sqrt expects a float");
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SinInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(CosInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SExtInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(i->sext(GetSize(ty) * 8));
      } else if (arg.IsFloat()) {
        return Lattice::Overdefined();
      } else if (arg.IsFloatZero()) {
        return Lattice::CreateInteger(0);
      } else if (arg.IsMask()) {
        return Lattice::Overdefined();
      }
      llvm_unreachable("cannot sext non-integer");
    }

    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(ZExtInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      const unsigned bits = GetBitWidth(ty);
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(i->zext(bits));
      } else if (auto f = arg.AsFloat()) {
        llvm::APSInt intVal(bits, false);
        bool isExact;
        f->convertToInteger(intVal, APFloat::rmTowardZero, &isExact);
        if (isExact) {
          return Lattice::CreateInteger(intVal);
        } else {
          return Lattice::Overdefined();
        }
      } else if (arg.IsFloatZero()) {
        return Lattice::CreateInteger(APInt(bits, 0, true));
      } else if (arg.IsMask()) {
        return Lattice::Overdefined();
      }
      llvm_unreachable("cannot zext non-integer");
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(XExtInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(i->zext(GetBitWidth(ty)));
      } else if (auto f = arg.AsFloat()) {
        llvm::APSInt intVal(GetBitWidth(ty), false);
        bool isExact;
        f->convertToInteger(intVal, APFloat::rmTowardZero, &isExact);
        if (isExact) {
          return Lattice::CreateInteger(intVal);
        } else {
          return Lattice::Overdefined();
        }
      } else if (arg.IsFloatZero()) {
        return Lattice::CreateInteger(0);
      } else if (arg.IsMask()) {
        return Lattice::Overdefined();
      }
      llvm_unreachable("cannot zext non-integer");
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}


// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(FExtInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128:  {
      llvm_unreachable("cannot fext integer");
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid instruction type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(TruncInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128:  {
      unsigned bitWidth = GetSize(ty) * 8;
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(i->trunc(bitWidth));
      } else if (arg.IsFloat()) {
        return Lattice::Overdefined();
      }
      return Lattice::Overdefined();
    }
    case Type::F64: case Type::F32: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(ExpInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      llvm_unreachable("cannot exp integer");
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(Exp2Inst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(LogInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(Log2Inst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(Log10Inst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(FCeilInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(FFloorInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(PopCountInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(ClzInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(CtzInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(ByteSwapInst *inst, Lattice &arg)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}


// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(AddInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::I16: case Type::I32: case Type::I128: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(*l + *r);
        }
      }
      return Lattice::Overdefined();
    }
    case Type::V64:
    case Type::I64: {
      switch (lhs.GetKind()) {
        case Lattice::Kind::UNKNOWN: {
          llvm_unreachable("invalid argument");
        }
        case Lattice::Kind::UNDEFINED: {
          return Lattice::Undefined();
        }
        case Lattice::Kind::INT: {
          auto l = lhs.GetInt();
          if (rhs.IsFrame()) {
            return Lattice::CreateFrame(
                rhs.GetFrameObject(),
                rhs.GetFrameOffset() + l.getSExtValue()
            );
          } else if (rhs.IsGlobal()) {
            return Lattice::CreateGlobal(
                rhs.GetGlobalSymbol(),
                rhs.GetGlobalOffset() + l.getSExtValue()
            );
          } else if (auto r = rhs.AsInt()) {
            return Lattice::CreateInteger(l + *r);
          } else {
            return Lattice::Overdefined();
          }
        }
        case Lattice::Kind::FRAME: {
          switch (rhs.GetKind()) {
            case Lattice::Kind::UNKNOWN: {
              llvm_unreachable("invalid argument");
            }
            case Lattice::Kind::UNDEFINED: {
              return Lattice::Undefined();
            }
            case Lattice::Kind::FRAME:
            case Lattice::Kind::GLOBAL:
            case Lattice::Kind::POINTER:
            case Lattice::Kind::RANGE: {
              llvm_unreachable("not implemented");
            }
            case Lattice::Kind::OVERDEFINED:
            case Lattice::Kind::MASK:
            case Lattice::Kind::FLOAT:
            case Lattice::Kind::FLOAT_ZERO: {
              return Lattice::Pointer();
            }
            case Lattice::Kind::INT: {
              return Lattice::CreateFrame(
                  lhs.GetFrameObject(),
                  lhs.GetFrameOffset() + rhs.GetInt().getSExtValue()
              );
            }
          }
          llvm_unreachable("invalid lattice kind");
        }
        case Lattice::Kind::GLOBAL: {
          switch (rhs.GetKind()) {
            case Lattice::Kind::UNKNOWN: {
              llvm_unreachable("invalid argument");
            }
            case Lattice::Kind::UNDEFINED: {
              return Lattice::Undefined();
            }
            case Lattice::Kind::FRAME:
            case Lattice::Kind::GLOBAL:
            case Lattice::Kind::POINTER:
            case Lattice::Kind::RANGE: {
              llvm_unreachable("not implemented");
            }
            case Lattice::Kind::OVERDEFINED:
            case Lattice::Kind::MASK:
            case Lattice::Kind::FLOAT:
            case Lattice::Kind::FLOAT_ZERO: {
              return Lattice::CreateRange(lhs.GetGlobalSymbol());
            }
            case Lattice::Kind::INT: {
              return Lattice::CreateGlobal(
                  lhs.GetGlobalSymbol(),
                  lhs.GetGlobalOffset() + rhs.GetInt().getSExtValue()
              );
            }
          }
          llvm_unreachable("invalid lattice kind");
        }
        case Lattice::Kind::POINTER: {
          switch (rhs.GetKind()) {
            case Lattice::Kind::UNKNOWN: {
              llvm_unreachable("invalid argument");
            }
            case Lattice::Kind::UNDEFINED: {
              return Lattice::Undefined();
            }
            case Lattice::Kind::FRAME:
            case Lattice::Kind::GLOBAL:
            case Lattice::Kind::POINTER:
            case Lattice::Kind::RANGE: {
              llvm_unreachable("not implemented");
            }
            case Lattice::Kind::OVERDEFINED:
            case Lattice::Kind::MASK:
            case Lattice::Kind::FLOAT:
            case Lattice::Kind::FLOAT_ZERO:
            case Lattice::Kind::INT: {
              return lhs;
            }
          }
          llvm_unreachable("invalid lattice kind");
        }
        case Lattice::Kind::RANGE: {
          switch (rhs.GetKind()) {
            case Lattice::Kind::UNKNOWN: {
              llvm_unreachable("invalid argument");
            }
            case Lattice::Kind::UNDEFINED: {
              return Lattice::Undefined();
            }
            case Lattice::Kind::FRAME:
            case Lattice::Kind::GLOBAL:
            case Lattice::Kind::POINTER:
            case Lattice::Kind::RANGE: {
              llvm_unreachable("not implemented");
            }
            case Lattice::Kind::OVERDEFINED:
            case Lattice::Kind::MASK:
            case Lattice::Kind::FLOAT:
            case Lattice::Kind::FLOAT_ZERO:
            case Lattice::Kind::INT: {
              return lhs;
            }
          }
          llvm_unreachable("invalid lattice kind");
        }
        case Lattice::Kind::OVERDEFINED:
        case Lattice::Kind::MASK:
        case Lattice::Kind::FLOAT:
        case Lattice::Kind::FLOAT_ZERO: {
          if (rhs.IsGlobal()) {
            return Lattice::CreateRange(rhs.GetGlobalSymbol());
          } else {
            return Lattice::Overdefined();
          }
        }
      }
      llvm_unreachable("invalid lattice kind");
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      if (lhs.IsFloatZero()) {
        return rhs;
      }
      if (rhs.IsFloatZero()) {
        return lhs;
      }
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SubInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::I16: case Type::I32: case Type::I128: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(*l - *r);
        }
      }
      return Lattice::Overdefined();
    }
    case Type::V64:
    case Type::I64: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(*l - *r);
        }
      } else if (lhs.IsFrame()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateFrame(
              lhs.GetFrameObject(),
              lhs.GetFrameOffset() - r->getSExtValue()
          );
        }
      } else if (lhs.IsGlobal()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateGlobal(
              lhs.GetGlobalSymbol(),
              lhs.GetGlobalOffset() - r->getSExtValue()
          );
        }
      }
      return Lattice::Overdefined();
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
static Lattice AndMask(const APInt &i, const APInt &known, const APInt &value)
{
  return Lattice::CreateMask(known, value & i & known);
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(AndInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(*l & *r);
        } else if (rhs.IsMask()) {
          return AndMask(*l, rhs.GetKnown(), rhs.GetValue());
        }
      } else if (lhs.IsMask()) {
        if (auto r = rhs.AsInt()) {
          return AndMask(*r, lhs.GetKnown(), lhs.GetValue());
        }
      }
      return Lattice::Overdefined();
    }
    case Type::V64:
    case Type::I64: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(*l & *r);
        } else if (rhs.IsMask()) {
          return AndMask(*l, rhs.GetKnown(), rhs.GetValue());
        }
      } else if (lhs.IsFrame()) {
        const Func *func = inst->getParent()->getParent();
        int64_t offset = lhs.GetFrameOffset();
        unsigned object = lhs.GetFrameObject();
        auto &obj = func->object(object);
        if (auto r = rhs.AsInt()) {
          // If the alignment, which is a power of two, is larger than the
          // mask, it means that all bits set in the mask are equal to the
          // corresponding bits of the offset. The values can be and-ed.
          uint64_t mask = r->getSExtValue();
          if (offset == 0 && mask < obj.Alignment.value()) {
            return Lattice::CreateInteger(0);
          }
          return Lattice::Overdefined();
        } else {
          return Lattice::Overdefined();
        }
      } else if (lhs.IsGlobal()) {
        int64_t offset = lhs.GetGlobalOffset();
        if (auto optAlign = lhs.GetGlobalSymbol()->GetAlignment()) {
          unsigned align = optAlign->value();
          if (auto r = rhs.AsInt()) {
            uint64_t mask = r->getSExtValue();
            if (offset == 0 && mask < align) {
              return Lattice::CreateInteger(0);
            }
            return Lattice::Overdefined();
          }
        } else {
          return Lattice::Overdefined();
        }
      } else if (lhs.IsMask()) {
        if (auto r = rhs.AsInt()) {
          return AndMask(*r, lhs.GetKnown(), lhs.GetValue());
        }
      }
      return Lattice::Overdefined();
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      llvm_unreachable("cannot and floats");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
static Lattice FrameOr(OrInst *i, unsigned obj, int64_t off, const APInt &v) {
  auto *func = i->getParent()->getParent();
  const auto &stackObj = func->object(obj);
  const auto align = stackObj.Alignment.value();
  const uint64_t value = v.getZExtValue();
  if (off % align == 0 && value < align) {
    return Lattice::CreateFrame(obj, off + value);
  }
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(OrInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::V64:
    case Type::I128: {
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          return Lattice::CreateInteger(*il | *ir);
        }
      }
      return Lattice::Overdefined();
    }

    case Type::I64: {
      const unsigned bits = GetSize(ty) * 8;
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          return Lattice::CreateInteger(*il | *ir);
        } else if (rhs.IsFrame()) {
          return FrameOr(inst, rhs.GetFrameObject(), rhs.GetFrameOffset(), *il);
        }
      } else if (lhs.IsFrame()) {
        if (auto ir = rhs.AsInt()) {
          return FrameOr(inst, lhs.GetFrameObject(), lhs.GetFrameOffset(), *ir);
        }
      }
      return Lattice::Overdefined();
    }

    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      llvm_unreachable("cannot or float types");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(XorInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          return Lattice::CreateInteger(*il ^ *ir);
        }
      }
      return Lattice::Overdefined();
    }

    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      llvm_unreachable("cannot xor float types");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(PowInst *inst, Lattice &lhs, Lattice &rhs)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(CopySignInst *inst, Lattice &lhs, Lattice &rhs)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(OUAddInst *inst, Lattice &lhs, Lattice &rhs)
{
  if (auto l = lhs.AsInt()) {
    if (auto r = rhs.AsInt()) {
      bool overflow;
      (void) l->uadd_ov(*r, overflow);
      return MakeBoolean(overflow, inst->GetType());
    }
  }
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(OUMulInst *inst, Lattice &lhs, Lattice &rhs)
{
  if (auto l = lhs.AsInt()) {
    if (auto r = rhs.AsInt()) {
      bool overflow;
      (void) l->umul_ov(*r, overflow);
      return MakeBoolean(overflow, inst->GetType());
    }
  }
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(OUSubInst *inst, Lattice &lhs, Lattice &rhs)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(OSAddInst *inst, Lattice &lhs, Lattice &rhs)
{
  if (auto l = lhs.AsInt()) {
    if (auto r = rhs.AsInt()) {
      bool overflow;
      (void) l->sadd_ov(*r, overflow);
      return MakeBoolean(overflow, inst->GetType());
    }
  }
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(OSMulInst *inst, Lattice &lhs, Lattice &rhs)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(OSSubInst *inst, Lattice &lhs, Lattice &rhs)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(UDivInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      auto bitWidth = GetSize(ty) * 8;
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          if (*ir != 0) {
            return Lattice::CreateInteger(il->udiv(*ir));
          }
          return Lattice::Undefined();
        }
      }
      return Lattice::Overdefined();
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SDivInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      auto bitWidth = GetSize(ty) * 8;
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          if (*ir != 0) {
            return Lattice::CreateInteger(il->sdiv(*ir));
          }
          return Lattice::Undefined();
        }
      }
      return Lattice::Overdefined();
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(URemInst *inst, Lattice &lhs, Lattice &rhs)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SRemInst *inst, Lattice &lhs, Lattice &rhs)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(MulInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      auto bitWidth = GetSize(ty) * 8;
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          return Lattice::CreateInteger(*il * *ir);
        }
      }
      return Lattice::Overdefined();
    }
    case Type::F32: case Type::F64: case Type::F80: case Type::F128: {
      if (auto il = lhs.AsFloat(); il && il->isZero()) {
        return Lattice::CreateFloatZero();
      }
      if (auto ir = rhs.AsFloat(); ir && ir->isZero()) {
        return Lattice::CreateFloatZero();
      }
      return Lattice::Overdefined();
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(MulHSInst *inst, Lattice &lhs, Lattice &rhs)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(MulHUInst *inst, Lattice &lhs, Lattice &rhs)
{
  // TODO: implement this rule
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(Bitwise kind, Type ty, Lattice &lhs, Lattice &rhs)
{
  auto si = rhs.AsInt();
  if (!si) {
    return Lattice::Overdefined();
  }
  const auto &b = *si;

  switch (ty) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      switch (lhs.GetKind()) {
        case Lattice::Kind::UNDEFINED:
        case Lattice::Kind::UNKNOWN: {
          return lhs;
        }
        case Lattice::Kind::OVERDEFINED: {
          switch (kind) {
            case Bitwise::SRL:
            case Bitwise::SRA:
            case Bitwise::ROTL:
            case Bitwise::ROTR: {
              return Lattice::Overdefined();
            }
            case Bitwise::SLL: {
              unsigned bits = GetBitWidth(ty);
              auto o = APInt(bits, 1, true);
              return Lattice::CreateMask((o << b) - o, APInt(bits, 0, true));
            }
          }
          llvm_unreachable("not a shift instruction");
        }
        case Lattice::Kind::INT: {
          auto i = lhs.GetInt();
          switch (kind) {
            case Bitwise::SRL:  return Lattice::CreateInteger(i.lshr(b));
            case Bitwise::SRA:  return Lattice::CreateInteger(i.ashr(b));
            case Bitwise::SLL:  return Lattice::CreateInteger(i.shl(b));
            case Bitwise::ROTL: return Lattice::CreateInteger(i.rotl(b));
            case Bitwise::ROTR: return Lattice::CreateInteger(i.rotr(b));
          }
          llvm_unreachable("not a shift instruction");
        }
        case Lattice::Kind::MASK: {
          const auto &k = lhs.GetKnown();
          const auto &v = lhs.GetValue();
          switch (kind) {
            case Bitwise::SRL:  return Lattice::CreateMask(k.lshr(b), v.lshr(b));
            case Bitwise::SRA:  return Lattice::CreateMask(k.ashr(b), v.ashr(b));
            case Bitwise::SLL:  return Lattice::CreateMask(k.shl(b), v.shl(b));
            case Bitwise::ROTL: return Lattice::CreateMask(k.rotl(b), v.rotl(b));
            case Bitwise::ROTR: return Lattice::CreateMask(k.rotr(b), v.rotr(b));
          }
          llvm_unreachable("not a shift instruction");
        }
        case Lattice::Kind::GLOBAL:
        case Lattice::Kind::FRAME:
        case Lattice::Kind::POINTER: {
          if (*si == 0) {
            return lhs;
          } else {
            switch (kind) {
              case Bitwise::SRL:
              case Bitwise::SRA:
              case Bitwise::ROTL:
              case Bitwise::ROTR: {
                return Lattice::Overdefined();
              }
              case Bitwise::SLL: {
                unsigned bits = GetBitWidth(ty);
                auto o = APInt(bits, 1, true);
                return Lattice::CreateMask((o << b) - o, APInt(bits, 0, true));
              }
            }
            llvm_unreachable("not a shift instruction");
          }
        }
        case Lattice::Kind::FLOAT:
        case Lattice::Kind::FLOAT_ZERO: {
          llvm_unreachable("cannot shift floats");
        }
        case Lattice::Kind::RANGE: {
          llvm_unreachable("not implemented");
        }
      }
      return Lattice::Overdefined();
    }
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("cannot shift floats");
    }
  }
  llvm_unreachable("invalid shift type");
}
