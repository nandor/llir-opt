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
