// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/sccp/lattice.h"
#include "passes/sccp/solver.h"



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
static bool Compare(const APFloat &lhs, const APFloat &rhs, Cond cc, Type ty)
{
  switch (cc) {
    case Cond::O: case Cond::UO: {
      switch (lhs.compare(rhs)) {
        case llvm::APFloatBase::cmpEqual:
        case llvm::APFloatBase::cmpLessThan:
        case llvm::APFloatBase::cmpGreaterThan:
          return cc == Cond::O;
        case llvm::APFloatBase::cmpUnordered:
          return cc == Cond::UO;
      }
      llvm_unreachable("invalid comparison result");
    }
    case Cond::EQ: case Cond::OEQ: case Cond::UEQ: {
      switch (lhs.compare(rhs)) {
        case llvm::APFloatBase::cmpEqual:
          return true;
        case llvm::APFloatBase::cmpLessThan:
        case llvm::APFloatBase::cmpGreaterThan:
          return false;
        case llvm::APFloatBase::cmpUnordered:
          return cc == Cond::UEQ;
      }
      llvm_unreachable("invalid comparison result");
    }
    case Cond::NE: case Cond::ONE: case Cond::UNE: {
      switch (lhs.compare(rhs)) {
        case llvm::APFloatBase::cmpEqual:
          return false;
        case llvm::APFloatBase::cmpLessThan:
        case llvm::APFloatBase::cmpGreaterThan:
          return true;
        case llvm::APFloatBase::cmpUnordered:
          return cc == Cond::UNE;
      }
      llvm_unreachable("invalid comparison result");
    }
    case Cond::LT: case Cond::OLT: case Cond::ULT:{
      switch (lhs.compare(rhs)) {
        case llvm::APFloatBase::cmpEqual:
        case llvm::APFloatBase::cmpGreaterThan:
          return false;
        case llvm::APFloatBase::cmpLessThan:
          return true;
        case llvm::APFloatBase::cmpUnordered:
          return cc == Cond::ULT;
      }
      llvm_unreachable("invalid comparison result");
    }
    case Cond::GT: case Cond::OGT: case Cond::UGT:{
      switch (lhs.compare(rhs)) {
        case llvm::APFloatBase::cmpEqual:
        case llvm::APFloatBase::cmpLessThan:
          return false;
        case llvm::APFloatBase::cmpGreaterThan:
          return true;
        case llvm::APFloatBase::cmpUnordered:
          return cc == Cond::UGT;
      }
      llvm_unreachable("invalid comparison result");
    }
    case Cond::LE: case Cond::OLE: case Cond::ULE:{
      switch (lhs.compare(rhs)) {
        case llvm::APFloatBase::cmpGreaterThan:
          return false;
        case llvm::APFloatBase::cmpLessThan:
        case llvm::APFloatBase::cmpEqual:
          return true;
        case llvm::APFloatBase::cmpUnordered:
          return cc == Cond::ULE;
      }
      llvm_unreachable("invalid comparison result");
    }
    case Cond::GE: case Cond::OGE: case Cond::UGE:{
      switch (lhs.compare(rhs)) {
        case llvm::APFloatBase::cmpLessThan:
          return false;
        case llvm::APFloatBase::cmpEqual:
        case llvm::APFloatBase::cmpGreaterThan:
          return true;
        case llvm::APFloatBase::cmpUnordered:
          return cc == Cond::UGE;
      }
      llvm_unreachable("invalid comparison result");
    }
  }
  llvm_unreachable("invalid condition code");
}

// -----------------------------------------------------------------------------
static bool Compare(const APInt &lhs, const APInt &rhs, Cond cc)
{
  switch (cc) {
    case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return lhs == rhs;
    case Cond::NE: case Cond::ONE: case Cond::UNE: return lhs != rhs;
    case Cond::LT: case Cond::OLT: return lhs.slt(rhs);
    case Cond::ULT:                return lhs.ult(rhs);
    case Cond::GT: case Cond::OGT: return lhs.sgt(rhs);
    case Cond::UGT:                return lhs.ugt(rhs);
    case Cond::LE: case Cond::OLE: return lhs.sle(rhs);
    case Cond::ULE:                return lhs.ule(rhs);
    case Cond::GE: case Cond::OGE: return lhs.sge(rhs);
    case Cond::UGE:                return lhs.uge(rhs);
    case Cond::O:
    case Cond::UO: llvm_unreachable("invalid integer code");
  }
  llvm_unreachable("invalid condition code");
}

// -----------------------------------------------------------------------------
static Lattice Compare(
    unsigned lobj,
    int64_t loff,
    unsigned robj,
    int64_t roff,
    Cond cc,
    Type ty)
{
  auto Flag = [ty](bool value) {
    return MakeBoolean(value, ty);
  };
  auto Cmp = [ty, &Flag, lobj, robj](bool value) {
    return (lobj == robj) ? Flag(value) : Lattice::Undefined();
  };

  bool equal = lobj == robj && loff == roff;
  switch (cc) {
    case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(equal);
    case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(!equal);
    case Cond::LT: case Cond::OLT: case Cond::ULT: return Cmp(loff < roff);
    case Cond::GT: case Cond::OGT: case Cond::UGT: return Cmp(loff > roff);
    case Cond::LE: case Cond::OLE: case Cond::ULE: return Cmp(loff <= roff);
    case Cond::GE: case Cond::OGE: case Cond::UGE: return Cmp(loff >= roff);
    case Cond::O: case Cond::UO: llvm_unreachable("invalid integer code");
  }
  llvm_unreachable("invalid condition code");
}

// -----------------------------------------------------------------------------
static Lattice Compare(
    Global *lg,
    int64_t loff,
    Global *rg,
    int64_t roff,
    Cond cc,
    Type ty)
{
  auto Flag = [ty](bool value) {
    return MakeBoolean(value, ty);
  };
  auto Cmp = [ty, &Flag, lg, rg](bool value) {
    return (lg == rg) ? Flag(value) : Lattice::Overdefined();
  };

  if (lg->IsWeak() || rg->IsWeak()) {
    return Lattice::Overdefined();
  }

  switch (lg->GetKind()) {
    case Global::Kind::EXTERN:
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Ordering is undefined for these types - equality is allowed.
      bool equal = lg == rg && loff == roff;
      switch (cc) {
        case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(equal);
        case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(!equal);
        default: return Lattice::Overdefined();
      }
      llvm_unreachable("invalid condition code");
    }
    case Global::Kind::ATOM: {
      switch (rg->GetKind()) {
        case Global::Kind::EXTERN:
        case Global::Kind::FUNC:
        case Global::Kind::BLOCK: {
          switch (cc) {
            case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(false);
            case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(true);
            default: return Lattice::Overdefined();
          }
          llvm_unreachable("invalid condition code");
        }
        case Global::Kind::ATOM: {
          auto *al = static_cast<Atom *>(lg);
          auto *ar = static_cast<Atom *>(rg);
          bool equal = lg == rg && loff == roff;
          switch (cc) {
            case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(equal);
            case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(!equal);
            case Cond::LT: case Cond::OLT: case Cond::ULT: return Cmp(loff < roff);
            case Cond::GT: case Cond::OGT: case Cond::UGT: return Cmp(loff > roff);
            case Cond::LE: case Cond::OLE: case Cond::ULE: return Cmp(loff <= roff);
            case Cond::GE: case Cond::OGE: case Cond::UGE: return Cmp(loff >= roff);
            case Cond::O: case Cond::UO: llvm_unreachable("invalid integer code");
          }
          llvm_unreachable("invalid condition code");
        }
      }
      llvm_unreachable("invalid global kind");
    }
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitCmpInst(CmpInst &inst)
{
  const auto &lhs = GetValue(inst.GetLHS());
  const auto &rhs = GetValue(inst.GetRHS());
  if (lhs.IsUnknown() || rhs.IsUnknown()) {
    return;
  }

  Cond cc = inst.GetCC();
  Type ty = inst.GetType();

  auto Unequal = [ty, cc] {
    switch (cc) {
      case Cond::EQ: case Cond::OEQ: case Cond::UEQ:
        return MakeBoolean(false, ty);
      case Cond::NE: case Cond::ONE: case Cond::UNE:
        return MakeBoolean(true, ty);
      default:
        return Lattice::Overdefined();
    }
    llvm_unreachable("invalid condition code");
  };

  auto IntOrder = [ty, cc] (bool Lower) {
    switch (cc) {
      case Cond::EQ: case Cond::OEQ: case Cond::UEQ:
        return MakeBoolean(false, ty);
      case Cond::NE: case Cond::ONE: case Cond::UNE:
        return MakeBoolean(true, ty);
      case Cond::LE: case Cond::OLE: case Cond::ULE:
      case Cond::LT: case Cond::OLT: case Cond::ULT:
        return MakeBoolean(Lower, ty);
      case Cond::GE: case Cond::OGE: case Cond::UGE:
      case Cond::GT: case Cond::OGT: case Cond::UGT:
        return MakeBoolean(!Lower, ty);
      case Cond::O: case Cond::UO:
        llvm_unreachable("invalid integer code");
    }
    llvm_unreachable("invalid condition code");
  };

  switch (lhs.GetKind()) {
    case Lattice::Kind::UNKNOWN: {
      llvm_unreachable("value cannot be compared");
    }
    case Lattice::Kind::OVERDEFINED:
    case Lattice::Kind::FLOAT_ZERO: {
      MarkOverdefined(inst);
      return;
    }
    case Lattice::Kind::UNDEFINED: {
      Mark(inst, Lattice::Undefined());
      return;
    }
    case Lattice::Kind::FLOAT: {
      switch (rhs.GetKind()) {
        case Lattice::Kind::UNKNOWN: {
          llvm_unreachable("value cannot be compared");
        }
        case Lattice::Kind::OVERDEFINED: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::INT:
        case Lattice::Kind::MASK:
        case Lattice::Kind::GLOBAL:
        case Lattice::Kind::FRAME:
        case Lattice::Kind::POINTER: {
          llvm_unreachable("value cannot be compared");
        }
        case Lattice::Kind::UNDEFINED: {
          Mark(inst, Lattice::Undefined());
          return;
        }
        case Lattice::Kind::FLOAT: {
          Mark(inst, Compare(lhs.GetFloat(), rhs.GetFloat(), cc, ty));
          return;
        }
        case Lattice::Kind::FLOAT_ZERO: {
          llvm_unreachable("not implemented");
        }
        case Lattice::Kind::RANGE: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case Lattice::Kind::INT: {
      switch (rhs.GetKind()) {
        case Lattice::Kind::FLOAT:
        case Lattice::Kind::FLOAT_ZERO:
        case Lattice::Kind::UNKNOWN:
        case Lattice::Kind::OVERDEFINED: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::UNDEFINED: {
          Mark(inst, Lattice::Undefined());
          return;
        }
        case Lattice::Kind::FRAME: {
          if (lhs.GetInt().isNullValue()) {
            Mark(inst, IntOrder(true));
          } else {
            MarkOverdefined(inst);
          }
          return;
        }
        case Lattice::Kind::GLOBAL: {
          auto *g = rhs.GetGlobalSymbol();
          if (lhs.GetInt().isNullValue()) {
            if (g->IsWeak()) {
              MarkOverdefined(inst);
            } else {
              Mark(inst, IntOrder(true));
            }
          } else {
            MarkOverdefined(inst);
          }
          return;
        }
        case Lattice::Kind::INT: {
          Mark(inst, MakeBoolean(Compare(lhs.GetInt(), rhs.GetInt(), cc), ty));
          return;
        }
        case Lattice::Kind::MASK: {
          auto mask = rhs.GetKnown() & (rhs.GetValue() ^ lhs.GetInt());
          if (mask.isNullValue()) {
            MarkOverdefined(inst);
          } else {
            Mark(inst, Unequal());
          }
          return;
        }
        case Lattice::Kind::POINTER: {
          if (lhs.GetInt().isNullValue()) {
            Mark(inst, IntOrder(true));
          } else {
            MarkOverdefined(inst);
          }
          return;
        }
        case Lattice::Kind::RANGE: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case Lattice::Kind::MASK: {
      switch (rhs.GetKind()) {
        case Lattice::Kind::UNDEFINED: {
          Mark(inst, Lattice::Undefined());
          return;
        }
        case Lattice::Kind::FLOAT:
        case Lattice::Kind::UNKNOWN:
        case Lattice::Kind::OVERDEFINED:
        case Lattice::Kind::FLOAT_ZERO:
        case Lattice::Kind::GLOBAL: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::INT: {
          auto knownLHS = lhs.GetKnown() & lhs.GetValue();
          auto knownRHS = lhs.GetKnown() & rhs.GetInt();
          if (knownLHS != knownRHS) {
            Mark(inst, Unequal());
          } else {
            MarkOverdefined(inst);
          }
          return;
        }
        case Lattice::Kind::MASK: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::FRAME: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::POINTER: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::RANGE: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case Lattice::Kind::FRAME: {
      switch (rhs.GetKind()) {
        case Lattice::Kind::FLOAT:
        case Lattice::Kind::UNKNOWN:
        case Lattice::Kind::OVERDEFINED:
        case Lattice::Kind::FLOAT_ZERO: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::UNDEFINED: {
          Mark(inst, Lattice::Undefined());
          return;
        }
        case Lattice::Kind::GLOBAL: {
          Mark(inst, Unequal());
          return;
        }
        case Lattice::Kind::INT: {
          if (rhs.GetInt().isNullValue()) {
            Mark(inst, IntOrder(false));
          } else {
            MarkOverdefined(inst);
          }
          return;
        }
        case Lattice::Kind::MASK: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::FRAME: {
          Mark(inst, Compare(
              lhs.GetFrameObject(),
              lhs.GetFrameOffset(),
              rhs.GetFrameObject(),
              rhs.GetFrameOffset(),
              cc,
              ty
          ));
          return;
        }
        case Lattice::Kind::POINTER: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::RANGE: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case Lattice::Kind::GLOBAL: {
      auto *g = lhs.GetGlobalSymbol();
      switch (rhs.GetKind()) {
        case Lattice::Kind::UNKNOWN:
        case Lattice::Kind::OVERDEFINED:
        case Lattice::Kind::FLOAT:
        case Lattice::Kind::FLOAT_ZERO: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::UNDEFINED: {
          Mark(inst, Lattice::Undefined());
          return;
        }
        case Lattice::Kind::FRAME:
        case Lattice::Kind::POINTER: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::GLOBAL: {
          Mark(inst, Compare(
              lhs.GetGlobalSymbol(),
              lhs.GetGlobalOffset(),
              rhs.GetGlobalSymbol(),
              rhs.GetGlobalOffset(),
              cc,
              ty
          ));
          return;
        }
        case Lattice::Kind::INT: {
          if (rhs.GetInt().isNullValue()) {
            Mark(inst, g->IsWeak() ? Lattice::Overdefined() : IntOrder(false));
          } else {
            MarkOverdefined(inst);
          }
          return;
        }
        case Lattice::Kind::MASK: {
          llvm_unreachable("not implemented");
        }
        case Lattice::Kind::RANGE: {
          auto *gl = lhs.GetGlobalSymbol();
          auto *gr = rhs.GetRange();
          if (gl == gr) {
            Mark(inst, rhs);
          } else {
            Mark(inst, Lattice::Pointer());
          }
          return;
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case Lattice::Kind::POINTER: {
      switch (rhs.GetKind()) {
        case Lattice::Kind::UNKNOWN:
        case Lattice::Kind::OVERDEFINED:
        case Lattice::Kind::FLOAT:
        case Lattice::Kind::FLOAT_ZERO: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::UNDEFINED: {
          Mark(inst, Lattice::Undefined());
          return;
        }
        case Lattice::Kind::FRAME:
        case Lattice::Kind::POINTER:
        case Lattice::Kind::GLOBAL: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::INT: {
          if (rhs.GetInt().isNullValue()) {
            Mark(inst, IntOrder(false));
          } else {
            MarkOverdefined(inst);
          }
          return;
        }
        case Lattice::Kind::MASK: {
          llvm_unreachable("not implemented");
        }
        case Lattice::Kind::RANGE: {
          MarkOverdefined(inst);
          return;
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
    case Lattice::Kind::RANGE: {
      switch (rhs.GetKind()) {
        case Lattice::Kind::UNKNOWN:
        case Lattice::Kind::OVERDEFINED:
        case Lattice::Kind::FLOAT:
        case Lattice::Kind::FLOAT_ZERO: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::UNDEFINED: {
          Mark(inst, Lattice::Undefined());
          return;
        }
        case Lattice::Kind::FRAME:
        case Lattice::Kind::POINTER:
        case Lattice::Kind::GLOBAL: {
          MarkOverdefined(inst);
          return;
        }
        case Lattice::Kind::INT: {
          if (rhs.GetInt().isNullValue()) {
            Mark(inst, IntOrder(false));
          } else {
            MarkOverdefined(inst);
          }
          return;
        }
        case Lattice::Kind::MASK: {
          llvm_unreachable("not implemented");
        }
        case Lattice::Kind::RANGE: {
          MarkOverdefined(inst);
          return;
        }
      }
      llvm_unreachable("invalid rhs kind");
    }
  }
  llvm_unreachable("invalid lhs kind");
}

