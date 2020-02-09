// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/func.h"
#include "passes/sccp/eval.h"



// -----------------------------------------------------------------------------
static APFloat extend(Type ty, const APFloat &f)
{
  switch (ty) {
    case Type::F32: {
      APFloat r = f;
      bool i;
      r.convert(
          llvm::APFloatBase::IEEEsingle(),
          APFloat::rmNearestTiesToEven,
          &i
      );
      return r;
    }
    case Type::F64: {
      APFloat r = f;
      bool i;
      r.convert(
          llvm::APFloatBase::IEEEdouble(),
          APFloat::rmNearestTiesToEven,
          &i
      );
      return r;
    }
    case Type::F80:
      llvm_unreachable("not implemented");
    default: {
      llvm_unreachable("not a float type");
    }
  }
}

// -----------------------------------------------------------------------------
static APSInt extend(Type ty, const APSInt &i)
{
  return APSInt(i.sextOrTrunc(GetSize(ty) * 8), IsUnsigned(ty));
}

// -----------------------------------------------------------------------------
static Lattice MakeBoolean(bool value, Type ty)
{
  switch (ty) {
    case Type::U8:
    case Type::U16:
    case Type::U32:
    case Type::U64:
    case Type::U128: {
      return Lattice::CreateInteger(
          APSInt(APInt(GetSize(ty) * 8, value, false), true)
      );
    }

    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128: {
      return Lattice::CreateInteger(
          APSInt(APInt(GetSize(ty) * 8, value, true), false)
      );
    }

    case Type::F32:
    case Type::F64:
    case Type::F80:
      llvm_unreachable("invalid comparison");
  }
  llvm_unreachable("invalid type");
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
    default:
      llvm_unreachable("not a unary instruction");
    case Inst::Kind::ABS:
      return Eval(static_cast<AbsInst *>(inst), arg);
    case Inst::Kind::NEG:
      return Eval(static_cast<NegInst *>(inst), arg);
    case Inst::Kind::SQRT:
      return Eval(static_cast<SqrtInst *>(inst), arg);
    case Inst::Kind::SIN:
      return Eval(static_cast<SinInst *>(inst), arg);
    case Inst::Kind::COS:
      return Eval(static_cast<CosInst *>(inst), arg);
    case Inst::Kind::SEXT:
      return Eval(static_cast<SExtInst *>(inst), arg);
    case Inst::Kind::ZEXT:
      return Eval(static_cast<ZExtInst *>(inst), arg);
    case Inst::Kind::FEXT:
      return Eval(static_cast<FExtInst *>(inst), arg);
    case Inst::Kind::TRUNC:
      return Eval(static_cast<TruncInst *>(inst), arg);
  }
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(BinaryInst *inst, Lattice &lhs, Lattice &rhs)
{
  assert(!lhs.IsUnknown() && "invalid lhs");
  assert(!rhs.IsUnknown() && "invalid rhs");
  if (lhs.IsOverdefined() || rhs.IsOverdefined()) {
    return Lattice::Overdefined();
  }
  if (lhs.IsUndefined() || rhs.IsUndefined()) {
    return Lattice::Undefined();
  }

  const auto ty = inst->GetType();
  switch (inst->GetKind()) {
    default:
      llvm_unreachable("not a binary instruction");

    // Bitwise operators.
    case Inst::Kind::SLL:  return Eval(Bitwise::SLL,  ty, lhs, rhs);
    case Inst::Kind::SRA:  return Eval(Bitwise::SRA,  ty, lhs, rhs);
    case Inst::Kind::SRL:  return Eval(Bitwise::SRL,  ty, lhs, rhs);
    case Inst::Kind::ROTL: return Eval(Bitwise::ROTL, ty, lhs, rhs);

    // Arithmetic or pointer offset.
    case Inst::Kind::ADD:
      return Eval(static_cast<AddInst *>(inst), lhs, rhs);
    case Inst::Kind::SUB:
      return Eval(static_cast<SubInst *>(inst), lhs, rhs);
    // Bitwise or pointer masking.
    case Inst::Kind::AND:
      return Eval(static_cast<AndInst *>(inst), lhs, rhs);
    case Inst::Kind::OR:
      return Eval(static_cast<OrInst *>(inst), lhs, rhs);
    case Inst::Kind::XOR:
      return Eval(static_cast<XorInst *>(inst), lhs, rhs);
    // Regular arithmetic.
    case Inst::Kind::POW:
      return Eval(static_cast<PowInst *>(inst), lhs, rhs);
    case Inst::Kind::COPYSIGN:
      return Eval(static_cast<CopySignInst *>(inst), lhs, rhs);
    case Inst::Kind::UADDO:
      return Eval(static_cast<AddUOInst *>(inst), lhs, rhs);
    case Inst::Kind::UMULO:
      return Eval(static_cast<MulUOInst *>(inst), lhs, rhs);
    case Inst::Kind::CMP:
      return Eval(static_cast<CmpInst *>(inst), lhs, rhs);
    case Inst::Kind::DIV:
      return Eval(static_cast<DivInst *>(inst), lhs, rhs);
    case Inst::Kind::REM:
      return Eval(static_cast<RemInst *>(inst), lhs, rhs);
    case Inst::Kind::MUL:
      return Eval(static_cast<MulInst *>(inst), lhs, rhs);
  }
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(AbsInst *inst, Lattice &arg)
{
  llvm_unreachable("AbsInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(NegInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::U8:
    case Type::I16: case Type::U16:
    case Type::I32: case Type::U32:
    case Type::I64: case Type::U64:
    case Type::I128: case Type::U128: {
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(-extend(ty, *i));
      }
      llvm_unreachable("cannot negate non-integer");
    }
    case Type::F64: case Type::F32: case Type::F80: {
      if (auto f = arg.AsFloat()) {
        return Lattice::CreateFloat(neg(extend(ty, *f)));
      }
      llvm_unreachable("cannot negate non-float");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SqrtInst *inst, Lattice &arg)
{
  llvm_unreachable("SqrtInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SinInst *inst, Lattice &arg)
{
  llvm_unreachable("SinInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(CosInst *inst, Lattice &arg)
{
  llvm_unreachable("CosInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SExtInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::U8:
    case Type::I16: case Type::U16:
    case Type::I32: case Type::U32:
    case Type::I64: case Type::U64:
    case Type::I128: case Type::U128: {
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(extend(ty, *i));
      }
      llvm_unreachable("cannot sext non-integer");
    }

    case Type::F32: case Type::F64: case Type::F80: {
      if (auto i = arg.AsInt()) {
        APFloat R = ty == Type::F32 ? APFloat(0.0f) : APFloat(0.0);
        R.convertFromAPInt(*i, i->isSigned(), APFloat::rmNearestTiesToEven);
        return Lattice::CreateFloat(R);
      }
      llvm_unreachable("cannot sext non-float");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(ZExtInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::U8:
    case Type::I16: case Type::U16:
    case Type::I32: case Type::U32:
    case Type::I64: case Type::U64:
    case Type::I128: case Type::U128: {
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(
            APSInt(i->zextOrTrunc(GetSize(ty) * 8), !IsSigned(ty))
        );
      }
      llvm_unreachable("cannot zext non-integer");
    }
    case Type::F32: case Type::F64: case Type::F80: {
      if (auto i = arg.AsInt()) {
        APFloat R = ty == Type::F32 ? APFloat(0.0f) : APFloat(0.0);
        R.convertFromAPInt(*i, false, APFloat::rmNearestTiesToEven);
        return Lattice::CreateFloat(R);
      }
      llvm_unreachable("cannot zext non-float");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(FExtInst *inst, Lattice &arg)
{
  llvm_unreachable("FExtInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(TruncInst *inst, Lattice &arg)
{
  switch (auto ty = inst->GetType()) {
    case Type::U8:
    case Type::U16:
    case Type::U32:
    case Type::U64:
    case Type::U128: {
      unsigned bitWidth = GetSize(ty) * 8;
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(i->trunc(bitWidth));
      } else if (auto f = arg.AsFloat()) {
        APSInt r(APInt(bitWidth, 0, false), true);
        bool exact;
        f->convertToInteger(r, APFloat::rmNearestTiesToEven, &exact);
        return Lattice::CreateInteger(r);
      }
      llvm_unreachable("cannot truncate non-integer");
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128:  {
      unsigned bitWidth = GetSize(ty) * 8;
      if (auto i = arg.AsInt()) {
        return Lattice::CreateInteger(i->trunc(bitWidth));
      } else if (auto f = arg.AsFloat()) {
        APSInt r(APInt(bitWidth, 0, true), false);
        bool exact;
        f->convertToInteger(r, APFloat::rmNearestTiesToEven, &exact);
        return Lattice::CreateInteger(r);
      }
      llvm_unreachable("cannot truncate non-integer");
    }
    case Type::F64: case Type::F32: case Type::F80: {
      if (auto f = arg.AsFloat()) {
        return Lattice::CreateFloat(extend(ty, *f));
      }
      llvm_unreachable("cannot truncate non-float");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(AddInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::I16: case Type::I32: case Type::I128:
    case Type::U8: case Type::U16: case Type::U32: case Type::U128: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(extend(ty, *l) + extend(ty, *r));
        }
      }
      llvm_unreachable("cannot add non-integers");
    }
    case Type::I64: case Type::U64: {
      if (auto l = lhs.AsInt()) {
        if (rhs.IsFrame()) {
          return Lattice::CreateFrame(
              rhs.GetFrameObject(),
              rhs.GetFrameOffset() + l->getExtValue()
          );
        } else if (rhs.IsGlobal()) {
          return Lattice::CreateGlobal(
              rhs.GetGlobalSymbol(),
              rhs.GetGlobalOffset() + l->getExtValue()
          );
        } else if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(l->extOrTrunc(64) + r->extOrTrunc(64));
        }
      } else if (lhs.IsFrame()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateFrame(
              lhs.GetFrameObject(),
              lhs.GetFrameOffset() + r->getExtValue()
          );
        }
      } else if (lhs.IsGlobal()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateGlobal(
              lhs.GetGlobalSymbol(),
              lhs.GetGlobalOffset() + r->getExtValue()
          );
        }
      }
      llvm_unreachable("cannot add non-integers");
    }
    case Type::F32: case Type::F64: case Type::F80: {
      llvm_unreachable("cannot add floats");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(SubInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::I16: case Type::I32: case Type::I128:
    case Type::U8: case Type::U16: case Type::U32: case Type::U128: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(extend(ty, *l) + extend(ty, *r));
        }
      }
      llvm_unreachable("cannot subtract non-integers");
    }
    case Type::I64: case Type::U64: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(l->extOrTrunc(64) - r->extOrTrunc(64));
        }
      } else if (lhs.IsFrame()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateFrame(
              lhs.GetFrameObject(),
              lhs.GetFrameOffset() - r->getExtValue()
          );
        }
      } else if (lhs.IsGlobal()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateGlobal(
              lhs.GetGlobalSymbol(),
              lhs.GetGlobalOffset() - r->getExtValue()
          );
        }
      }
      llvm_unreachable("cannot subtract non-integers");
    }
    case Type::F32: case Type::F64: case Type::F80: {
      llvm_unreachable("cannot subtract floats");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(AndInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::U8:
    case Type::I16: case Type::U16:
    case Type::I32: case Type::U32:
    case Type::I128: case Type::U128: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(extend(ty, *l) & extend(ty, *r));
        }
      }
      llvm_unreachable("cannot and non-integers");
    }
    case Type::I64: case Type::U64: {
      if (auto l = lhs.AsInt()) {
        if (auto r = rhs.AsInt()) {
          return Lattice::CreateInteger(extend(ty, *l) & extend(ty, *r));
        }
      } else if (lhs.IsGlobal()) {
        if (auto r = rhs.AsInt()) {
          if (*r < 8) {
            return Lattice::CreateInteger(0);
          }
        }
      }

      llvm_unreachable("cannot and non-integers");
    }
    case Type::F32: case Type::F64: case Type::F80: {
      llvm_unreachable("cannot and floats");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
static Lattice FrameOr(OrInst *i, unsigned obj, int64_t off, const APSInt &v) {
  auto *func = i->getParent()->getParent();
  const auto &stackObj = func->object(obj);
  const auto align = stackObj.Alignment;
  if (off % align == 0 && v.getExtValue() < align) {
    return Lattice::CreateFrame(obj, off + v.getExtValue());
  }
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(OrInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::U8:
    case Type::I16: case Type::U16:
    case Type::I32: case Type::U32:
    case Type::I128: case Type::U128: {
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          return Lattice::CreateInteger(extend(ty, *il) | extend(ty, *ir));
        }
      }
      llvm_unreachable("cannot or non-integers");
    }

    case Type::U64: case Type::I64: {
      const unsigned bits = GetSize(ty) * 8;
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          return Lattice::CreateInteger(extend(ty, *il) | extend(ty, *ir));
        } else if (rhs.IsFrame()) {
          return FrameOr(inst, rhs.GetFrameObject(), rhs.GetFrameOffset(), *il);
        }
      } else if (lhs.IsFrame()) {
        if (auto ir = rhs.AsInt()) {
          return FrameOr(inst, lhs.GetFrameObject(), lhs.GetFrameOffset(), *ir);
        }
      }
      llvm_unreachable("cannot or non-integers or frames");
    }

    case Type::F32: case Type::F64: case Type::F80: {
      llvm_unreachable("cannot or float types");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(XorInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::U8:
    case Type::I16: case Type::U16:
    case Type::I32: case Type::U32:
    case Type::I64: case Type::U64:
    case Type::I128: case Type::U128: {
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          return Lattice::CreateInteger(extend(ty, *il) ^ extend(ty, *ir));
        }
      }
      llvm_unreachable("cannot xor non-integer types");
    }

    case Type::F32: case Type::F64: case Type::F80: {
      llvm_unreachable("cannot xor float types");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(PowInst *inst, Lattice &lhs, Lattice &rhs)
{
  llvm_unreachable("PowInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(CopySignInst *inst, Lattice &lhs, Lattice &rhs)
{
  llvm_unreachable("CopySignInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(AddUOInst *inst, Lattice &lhs, Lattice &rhs)
{
  if (auto l = lhs.AsInt()) {
    if (auto r = rhs.AsInt()) {
      unsigned bitWidth = std::max(l->getBitWidth(), r->getBitWidth());
      APSInt result = l->extend(bitWidth + 1) + r->extend(bitWidth + 1);
      bool overflow = result.trunc(bitWidth).extend(bitWidth + 1) != result;
      return MakeBoolean(overflow, inst->GetType());
    }
  }
  llvm_unreachable("AddUOInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(MulUOInst *inst, Lattice &lhs, Lattice &rhs)
{
  llvm_unreachable("MulUOInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(CmpInst *inst, Lattice &lhs, Lattice &rhs)
{
  using Ordering = Lattice::Ordering;
  using Equality = Lattice::Equality;

  auto ty = inst->GetType();
  switch (auto cc = inst->GetCC()) {
    case Cond::EQ:
    case Cond::NE: {
      switch (auto eq = Lattice::Equal(lhs, rhs)) {
        case Equality::OVERDEFINED:
          return Lattice::Overdefined();
        case Equality::UNDEFINED:
          return Lattice::Undefined();
        case Equality::EQUAL:
          return MakeBoolean(cc == Cond::EQ, ty);
        case Equality::UNEQUAL:
          return MakeBoolean(cc == Cond::NE, ty);
      }
      llvm_unreachable("invalid comparison");
    }
    default: {
      switch (auto ord = Lattice::Order(lhs, rhs)) {
        case Ordering::OVERDEFINED:
          return Lattice::Overdefined();
        case Ordering::UNDEFINED:
          return Lattice::Undefined();
        default: {
          switch (cc) {
            case Cond::OEQ: case Cond::UEQ:
              return MakeBoolean(
                  (cc == Cond::UEQ && ord == Ordering::UNORDERED) ||
                  ord == Ordering::EQUAL,
                  ty
              );
            case Cond::ONE: case Cond::UNE:
              return MakeBoolean(
                  (cc == Cond::UNE && ord == Ordering::UNORDERED) ||
                  (ord != Ordering::EQUAL),
                  ty
              );
            case Cond::LT: case Cond::OLT: case Cond::ULT:
              return MakeBoolean(
                  (cc == Cond::ULT && ord == Ordering::UNORDERED) ||
                  ord == Ordering::LESS,
                  ty
              );
            case Cond::GT: case Cond::OGT: case Cond::UGT:
              return MakeBoolean(
                  (cc == Cond::UGT && ord == Ordering::UNORDERED) ||
                  ord == Ordering::GREATER,
                  ty
              );
            case Cond::LE: case Cond::OLE: case Cond::ULE:
              return MakeBoolean(
                  (cc == Cond::ULE && ord == Ordering::UNORDERED) ||
                  ord == Ordering::LESS ||
                  ord == Ordering::EQUAL,
                  ty
              );
            case Cond::GE: case Cond::OGE: case Cond::UGE:
              return MakeBoolean(
                  (cc == Cond::UGE && ord == Ordering::UNORDERED) ||
                  ord == Ordering::GREATER ||
                  ord == Ordering::EQUAL,
                  ty
              );
            default:
              llvm_unreachable("invalid opcode");
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(DivInst *inst, Lattice &lhs, Lattice &rhs)
{
  llvm_unreachable("DivInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(RemInst *inst, Lattice &lhs, Lattice &rhs)
{
  llvm_unreachable("RemInst");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(MulInst *inst, Lattice &lhs, Lattice &rhs)
{
  switch (auto ty = inst->GetType()) {
    case Type::I8: case Type::U8:
    case Type::I16: case Type::U16:
    case Type::I32: case Type::U32:
    case Type::I64: case Type::U64:
    case Type::I128: case Type::U128: {
      auto bitWidth = GetSize(ty) * 8;
      if (auto il = lhs.AsInt()) {
        if (auto ir = rhs.AsInt()) {
          return Lattice::CreateInteger(extend(ty, *il) * extend(ty, *ir));
        }
      }
      llvm_unreachable("cannot multiply non-integers");
    }
    case Type::F32: case Type::F64: case Type::F80: {
      if (auto fl = lhs.AsFloat()) {
        if (auto fr = rhs.AsFloat()) {
          return Lattice::CreateFloat(extend(ty, *fl) * extend(ty, *fr));
        }
      }
      llvm_unreachable("cannot multiply non-floats");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
Lattice SCCPEval::Eval(Bitwise kind, Type ty, Lattice &lhs, Lattice &rhs)
{
  if (auto si = rhs.AsInt()) {
    switch (ty) {
      case Type::U8:
      case Type::U16:
      case Type::U32:
      case Type::U64:
      case Type::U128: {
        if (auto i = lhs.AsInt()) {
          auto iExt = i->zextOrTrunc(GetSize(ty) * 8);
          switch (kind) {
            case Bitwise::SRL:
              return Lattice::CreateInteger(APSInt(iExt.lshr(*si), true));
            case Bitwise::SRA:
              return Lattice::CreateInteger(APSInt(iExt.ashr(*si), true));
            case Bitwise::SLL:
              return Lattice::CreateInteger(APSInt(iExt.shl(*si), true));
            case Bitwise::ROTL:
              return Lattice::CreateInteger(APSInt(iExt.rotl(*si), true));
            default:
              llvm_unreachable("not a shift instruction");
          }
        }
        llvm_unreachable("invalid shift argument");
      }
      case Type::I8:
      case Type::I16:
      case Type::I32:
      case Type::I64:
      case Type::I128: {
        if (auto i = lhs.AsInt()) {
          auto iExt = i->sextOrTrunc(GetSize(ty) * 8);
          switch (kind) {
            case Bitwise::SRL:
              return Lattice::CreateInteger(APSInt(iExt.lshr(*si), false));
            case Bitwise::SRA:
              return Lattice::CreateInteger(APSInt(iExt.ashr(*si), false));
            case Bitwise::SLL:
              return Lattice::CreateInteger(APSInt(iExt.shl(*si), false));
            case Bitwise::ROTL:
              return Lattice::CreateInteger(APSInt(iExt.rotl(*si), false));
            default:
              llvm_unreachable("not a shift instruction");
          }
        }
        llvm_unreachable("invalid shift argument");
      }
      case Type::F32:
      case Type::F64:
      case Type::F80:
        llvm_unreachable("invalid shift result");
    }
  }
  llvm_unreachable("invalid shift amount");
}
