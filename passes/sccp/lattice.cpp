// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/global.h"
#include "passes/sccp/lattice.h"



// -----------------------------------------------------------------------------
Lattice::Lattice(const Lattice &that)
  : kind_(that.kind_)
{
  switch (kind_) {
    case Kind::INT:
      new (&intVal_) APSInt(that.intVal_);
      break;

    case Kind::FLOAT:
      new (&floatVal_) APFloat(that.floatVal_);
      break;

    case Kind::FRAME:
      frameVal_ = that.frameVal_;
      break;

    case Kind::GLOBAL:
      globalVal_ = that.globalVal_;
      break;

    case Kind::UNDEFINED:
    case Kind::OVERDEFINED:
    case Kind::UNKNOWN:
      break;
  }
}

// -----------------------------------------------------------------------------
Lattice::~Lattice()
{
  switch (kind_) {
    case Kind::INT:
      intVal_.~APSInt();
      break;

    case Kind::FLOAT:
      floatVal_.~APFloat();
      break;

    case Kind::FRAME:
    case Kind::GLOBAL:
      break;

    case Kind::UNDEFINED:
    case Kind::OVERDEFINED:
    case Kind::UNKNOWN:
      break;
  }
}

// -----------------------------------------------------------------------------
bool Lattice::IsTrue() const
{
  switch (kind_) {
    case Kind::INT:
      return intVal_.getBoolValue();
    case Kind::FLOAT:
      return !floatVal_.isZero();
    case Kind::FRAME:
    case Kind::GLOBAL:
      return true;
    case Kind::UNDEFINED:
    case Kind::OVERDEFINED:
      return false;
    case Kind::UNKNOWN:
      llvm_unreachable("invalid lattice value");
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
bool Lattice::IsFalse() const
{
  switch (kind_) {
    case Kind::INT:
      return !intVal_.getBoolValue();
    case Kind::FLOAT:
      return floatVal_.isZero();
    case Kind::FRAME:
    case Kind::GLOBAL:
      return false;
    case Kind::UNDEFINED:
    case Kind::OVERDEFINED:
      return false;
    case Kind::UNKNOWN:
      llvm_unreachable("invalid lattice value");
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
bool Lattice::operator == (const Lattice &that) const
{
  if (kind_ != that.kind_) {
    return false;
  }

  switch (kind_) {
    case Kind::UNKNOWN:
    case Kind::UNDEFINED:
    case Kind::OVERDEFINED:
      return true;

    case Kind::INT:
      return APSInt::compareValues(intVal_, that.intVal_) == 0;

    case Kind::FLOAT:
      return floatVal_.bitwiseIsEqual(that.floatVal_);

    case Kind::FRAME: {
      auto &fa = frameVal_, &fb = that.frameVal_;
      return fa.Obj == fb.Obj && fa.Off == fb.Off;
    }

    case Kind::GLOBAL: {
      auto &ga = globalVal_, &gb = that.globalVal_;
      return ga.Sym == gb.Sym && ga.Off == gb.Off;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
Lattice &Lattice::operator = (const Lattice &that)
{
  switch (kind_) {
    case Kind::INT:
      intVal_.~APSInt();
      break;

    case Kind::FLOAT:
      floatVal_.~APFloat();
      break;

    case Kind::FRAME:
    case Kind::GLOBAL:
      break;

    case Kind::UNDEFINED:
    case Kind::OVERDEFINED:
    case Kind::UNKNOWN:
      break;
  }

  kind_ = that.kind_;

  switch (kind_) {
    case Kind::INT:
      new (&intVal_) APSInt(that.intVal_);
      break;

    case Kind::FLOAT:
      new (&floatVal_) APFloat(that.floatVal_);
      break;

    case Kind::FRAME:
      frameVal_ = that.frameVal_;
      break;

    case Kind::GLOBAL:
      globalVal_ = that.globalVal_;
      break;

    case Kind::UNDEFINED:
    case Kind::OVERDEFINED:
    case Kind::UNKNOWN:
      break;
  }
  return *this;
}

// -----------------------------------------------------------------------------
Lattice::Equality Lattice::Equal(Lattice &LHS, Lattice &RHS)
{
  switch (LHS.kind_) {
    case Kind::UNKNOWN:
    case Kind::OVERDEFINED:
      llvm_unreachable("value cannot be compared");

    case Kind::UNDEFINED:
      return Equality::UNDEFINED;

    case Kind::FLOAT: {
      switch (RHS.GetKind()) {
        case Kind::UNKNOWN:
        case Kind::OVERDEFINED:
        case Kind::INT:
        case Kind::GLOBAL:
        case Kind::FRAME:
          llvm_unreachable("value cannot be compared");

        case Kind::UNDEFINED:
          return Equality::UNDEFINED;

        case Kind::FLOAT: {
          switch (LHS.floatVal_.compare(RHS.GetFloat())) {
            case llvm::APFloatBase::cmpEqual:
              return Equality::EQUAL;
            case llvm::APFloatBase::cmpLessThan:
            case llvm::APFloatBase::cmpGreaterThan:
            case llvm::APFloatBase::cmpUnordered:
              return Equality::UNEQUAL;
          }
        }
      }
      break;
    }

    case Kind::INT: {
      switch (RHS.GetKind()) {
        case Kind::FLOAT:
        case Kind::UNKNOWN:
        case Kind::OVERDEFINED:
          llvm_unreachable("value cannot be compared");

        case Kind::UNDEFINED:
          return Equality::UNDEFINED;

        case Kind::FRAME:
        case Kind::GLOBAL: {
          if (LHS.GetInt().isNullValue()) {
            return Equality::UNEQUAL;
          } else {
            return Equality::UNDEFINED;
          }
        }

        case Kind::INT: {
          if (APSInt::compareValues(LHS.GetInt(), RHS.GetInt()) == 0) {
            return Equality::EQUAL;
          } else {
            return Equality::UNEQUAL;
          }
        }
      }
      break;
    }
    case Kind::FRAME: {
      switch (RHS.GetKind()) {
        case Kind::FLOAT:
        case Kind::UNKNOWN:
        case Kind::OVERDEFINED:
          llvm_unreachable("value cannot be compared");

        case Kind::UNDEFINED:
          return Equality::UNDEFINED;

        case Kind::INT:
        case Kind::GLOBAL:
          return Equality::UNEQUAL;

        case Kind::FRAME: {
          if (LHS.GetFrameObject() != RHS.GetFrameObject()) {
            return Equality::UNEQUAL;
          }
          auto lOff = LHS.GetFrameOffset();
          auto rOff = RHS.GetFrameOffset();
          return lOff == rOff ? Equality::EQUAL : Equality::UNEQUAL;
        }
      }
      break;
    }
    case Kind::GLOBAL: {
      switch (RHS.GetKind()) {
        case Kind::UNKNOWN:
        case Kind::OVERDEFINED:
        case Kind::FLOAT:
          llvm_unreachable("value cannot be compared");

        case Kind::UNDEFINED:
          return Equality::UNDEFINED;
        case Kind::FRAME:
          return Equality::UNEQUAL;

        case Kind::GLOBAL: {
          auto *sl = LHS.GetGlobalSymbol();
          auto *sr = RHS.GetGlobalSymbol();
          if (sl == sr)
            return Equality::EQUAL;

          switch (sl->GetKind()) {
            case Global::Kind::EXTERN:
            case Global::Kind::FUNC:
            case Global::Kind::BLOCK:
              return Equality::UNEQUAL;

            case Global::Kind::ATOM: {
              switch (sr->GetKind()) {
                case Global::Kind::EXTERN:
                case Global::Kind::FUNC:
                case Global::Kind::BLOCK:
                  return Equality::UNEQUAL;

                case Global::Kind::ATOM: {
                  auto *al = static_cast<Atom *>(sl);
                  auto *ar = static_cast<Atom *>(sr);
                  if (al != ar) {
                    return Equality::UNEQUAL;
                  } else {
                    auto lOff = LHS.GetGlobalOffset();
                    auto rOff = RHS.GetGlobalOffset();
                    return lOff == rOff ? Equality::EQUAL : Equality::UNEQUAL;
                  }
                }
              }
            }
          }
        }

        case Kind::INT: {
          return Equality::UNEQUAL;
        }
      }
      break;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
Lattice::Ordering Lattice::Order(Lattice &LHS, Lattice &RHS)
{
  switch (LHS.kind_) {
    case Kind::UNKNOWN:
    case Kind::OVERDEFINED:
      llvm_unreachable("value cannot be compared");

    case Kind::UNDEFINED:
      return Ordering::UNDEFINED;

    case Kind::FLOAT: {
      switch (RHS.GetKind()) {
        case Kind::UNKNOWN:
        case Kind::OVERDEFINED:
        case Kind::INT:
        case Kind::GLOBAL:
        case Kind::FRAME:
          llvm_unreachable("value cannot be compared");

        case Kind::UNDEFINED:
          return Ordering::UNDEFINED;

        case Kind::FLOAT: {
          switch (LHS.floatVal_.compare(RHS.GetFloat())) {
            case llvm::APFloatBase::cmpLessThan:    return Ordering::LESS;
            case llvm::APFloatBase::cmpEqual:       return Ordering::EQUAL;
            case llvm::APFloatBase::cmpGreaterThan: return Ordering::GREATER;
            case llvm::APFloatBase::cmpUnordered:   return Ordering::UNORDERED;
          }
        }
      }
      break;
    }

    case Kind::INT: {
      switch (RHS.GetKind()) {
        case Kind::FLOAT:
        case Kind::UNKNOWN:
        case Kind::OVERDEFINED:
          llvm_unreachable("value cannot be compared");

        case Kind::UNDEFINED:
          return Ordering::UNDEFINED;

        case Kind::FRAME:
        case Kind::GLOBAL: {
          if (LHS.GetInt().isNullValue()) {
            return Ordering::LESS;
          } else {
            return Ordering::UNORDERED;
          }
        }

        case Kind::INT: {
          int order = APSInt::compareValues(LHS.GetInt(), RHS.GetInt());
          if (order < 0) {
            return Ordering::LESS;
          } else if (order > 0) {
            return Ordering::GREATER;
          } else {
            return Ordering::EQUAL;
          }
        }
      }
      break;
    }
    case Kind::FRAME: {
      switch (RHS.GetKind()) {
        case Kind::FLOAT:
        case Kind::UNKNOWN:
        case Kind::OVERDEFINED:
          llvm_unreachable("value cannot be compared");

        case Kind::UNDEFINED:
        case Kind::GLOBAL:
          return Ordering::UNDEFINED;

        case Kind::INT: {
          if (RHS.GetInt().isNullValue()) {
            return Ordering::GREATER;
          } else {
            return Ordering::UNORDERED;
          }
        }

        case Kind::FRAME: {
          if (LHS.GetFrameObject() != RHS.GetFrameObject()) {
            return Ordering::UNDEFINED;
          }
          auto lOff = LHS.GetFrameOffset();
          auto rOff = RHS.GetFrameOffset();
          if (lOff < rOff) {
            return Ordering::LESS;
          } else if (lOff > rOff) {
            return Ordering::GREATER;
          } else {
            return Ordering::EQUAL;
          }
        }
      }
      break;
    }
    case Kind::GLOBAL: {
      switch (RHS.GetKind()) {
        case Kind::UNKNOWN:
        case Kind::OVERDEFINED:
        case Kind::FLOAT:
          llvm_unreachable("value cannot be compared");

        case Kind::UNDEFINED:
        case Kind::FRAME:
          return Ordering::UNDEFINED;

        case Kind::GLOBAL: {
          auto *sl = LHS.GetGlobalSymbol();
          auto *sr = RHS.GetGlobalSymbol();
          switch (sl->GetKind()) {
            case Global::Kind::EXTERN:
            case Global::Kind::FUNC:
            case Global::Kind::BLOCK:
              return Ordering::OVERDEFINED;

            case Global::Kind::ATOM: {
              switch (sr->GetKind()) {
                case Global::Kind::EXTERN:
                case Global::Kind::FUNC:
                case Global::Kind::BLOCK:
                  return Ordering::OVERDEFINED;

                case Global::Kind::ATOM: {
                  auto *al = static_cast<Atom *>(sl);
                  auto *ar = static_cast<Atom *>(sr);
                  if (al == ar) {
                    auto lOff = LHS.GetGlobalOffset();
                    auto rOff = RHS.GetGlobalOffset();
                    if (lOff < rOff) {
                      return Ordering::LESS;
                    } else if (lOff > rOff) {
                      return Ordering::GREATER;
                    } else {
                      return Ordering::EQUAL;
                    }
                  }
                  return Ordering::UNDEFINED;
                }
              }
            }
          }
        }

        case Kind::INT: {
          if (RHS.GetInt().isNullValue()) {
            return Ordering::GREATER;
          } else {
            return Ordering::UNORDERED;
          }
        }
      }
      break;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
Lattice Lattice::Unknown()
{
  Lattice v(Kind::UNKNOWN);
  return v;
}

// -----------------------------------------------------------------------------
Lattice Lattice::Overdefined()
{
  Lattice v(Kind::OVERDEFINED);
  return v;
}

// -----------------------------------------------------------------------------
Lattice Lattice::Undefined()
{
  Lattice v(Kind::UNDEFINED);
  return v;
}

// -----------------------------------------------------------------------------
Lattice Lattice::CreateFrame(unsigned obj, int64_t off)
{
  Lattice v(Kind::FRAME);
  v.frameVal_.Obj = obj;
  v.frameVal_.Off = off;
  return v;
}

// -----------------------------------------------------------------------------
Lattice Lattice::CreateGlobal(Global *g, int64_t off)
{
  Lattice v(Kind::GLOBAL);
  v.globalVal_.Sym = g;
  v.globalVal_.Off = off;
  return v;
}

// -----------------------------------------------------------------------------
Lattice Lattice::CreateInteger(int64_t i)
{
  return Lattice::CreateInteger(APSInt(APInt(64, i, true), false));
}

// -----------------------------------------------------------------------------
Lattice Lattice::CreateInteger(const APSInt &i)
{
  Lattice v(Kind::INT);
  new (&v.intVal_) APSInt(i);
  return v;
}

// -----------------------------------------------------------------------------
Lattice Lattice::CreateFloat(double f)
{
  return Lattice::CreateFloat(APFloat(f));
}

// -----------------------------------------------------------------------------
Lattice Lattice::CreateFloat(const APFloat &f)
{
  Lattice v(Kind::FLOAT);
  new (&v.floatVal_) APFloat(f);
  return v;
}

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, const Lattice &l)
{
  switch (l.GetKind()) {
    case Lattice::Kind::UNKNOWN: {
      OS << "unknown";
      break;
    }
    case Lattice::Kind::OVERDEFINED: {
      OS << "overdefined";
      break;
    }
    case Lattice::Kind::INT: {
      OS << "int{" << l.GetInt() << "}";
      break;
    }
    case Lattice::Kind::FLOAT: {
      llvm::SmallVector<char, 16> buffer;
      l.GetFloat().toString(buffer);
      OS << "float{" << buffer << "}";
      break;
    }
    case Lattice::Kind::FRAME: {
      OS << "frame{" << l.GetFrameObject() << ", " << l.GetFrameOffset() << "}";
      break;
    }
    case Lattice::Kind::GLOBAL: {
      const auto &name = l.GetGlobalSymbol()->getName();
      OS << "global{" << name << " + " << l.GetGlobalOffset() << "}";
      break;
    }
    case Lattice::Kind::UNDEFINED: {
      OS << "undefined";
      break;
    }
  }
  return OS;
}
