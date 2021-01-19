// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cond.h"



// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Cond cc)
{
  switch (cc) {
    case Cond::EQ:  return os << "eq";
    case Cond::OEQ: return os << "oeq";
    case Cond::UEQ: return os << "ueq";
    case Cond::NE:  return os << "ne";
    case Cond::ONE: return os << "one";
    case Cond::UNE: return os << "une";
    case Cond::LT:  return os << "lt";
    case Cond::OLT: return os << "olt";
    case Cond::ULT: return os << "ult";
    case Cond::GT:  return os << "gt";
    case Cond::OGT: return os << "ogt";
    case Cond::UGT: return os << "ugt";
    case Cond::LE:  return os << "le";
    case Cond::OLE: return os << "ole";
    case Cond::ULE: return os << "ule";
    case Cond::GE:  return os << "ge";
    case Cond::OGE: return os << "oge";
    case Cond::UGE: return os << "uge";
    case Cond::O:   return os << "o";
    case Cond::UO:  return os << "uo";
  }
  llvm_unreachable("invalid condition code");
}

// -----------------------------------------------------------------------------
Cond GetInverseCond(Cond cc)
{
  switch (cc) {
    case Cond::EQ:  return Cond::NE;
    case Cond::OEQ: return Cond::ONE;
    case Cond::UEQ: return Cond::UNE;
    case Cond::NE:  return Cond::EQ;
    case Cond::ONE: return Cond::OEQ;
    case Cond::UNE: return Cond::OEQ;
    case Cond::LT:  return Cond::GE;
    case Cond::OLT: return Cond::OGE;
    case Cond::ULT: return Cond::UGE;
    case Cond::GT:  return Cond::LE;
    case Cond::OGT: return Cond::OLE;
    case Cond::UGT: return Cond::ULE;
    case Cond::LE:  return Cond::GT;
    case Cond::OLE: return Cond::OGT;
    case Cond::ULE: return Cond::UGT;
    case Cond::GE:  return Cond::LT;
    case Cond::OGE: return Cond::OLT;
    case Cond::UGE: return Cond::ULT;
    case Cond::O:   return Cond::UO;
    case Cond::UO:  return Cond::O;
  }
  llvm_unreachable("invalid condition code");
}
