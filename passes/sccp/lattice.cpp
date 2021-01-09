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
      new (&intVal_) APInt(that.intVal_);
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
    case Kind::POINTER:
    case Kind::FLOAT_ZERO: {
      break;
    }
  }
}

// -----------------------------------------------------------------------------
Lattice::~Lattice()
{
  switch (kind_) {
    case Kind::INT:
      intVal_.~APInt();
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
    case Kind::POINTER:
    case Kind::FLOAT_ZERO: {
      break;
    }
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
    case Kind::POINTER:
      return true;
    case Kind::UNDEFINED:
    case Kind::OVERDEFINED:
    case Kind::FLOAT_ZERO:
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
    case Kind::FLOAT_ZERO:
      return true;
    case Kind::FRAME:
    case Kind::GLOBAL:
    case Kind::POINTER:
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
    case Kind::POINTER:
    case Kind::FLOAT_ZERO: {
      return true;
    }

    case Kind::INT: {
      return intVal_ == that.intVal_;
    }

    case Kind::FLOAT: {
      return floatVal_.bitwiseIsEqual(that.floatVal_);
    }

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
      intVal_.~APInt();
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
    case Kind::POINTER:
    case Kind::FLOAT_ZERO:
      break;
  }

  kind_ = that.kind_;

  switch (kind_) {
    case Kind::INT:
      new (&intVal_) APInt(that.intVal_);
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
    case Kind::POINTER:
    case Kind::FLOAT_ZERO:
      break;
  }
  return *this;
}

// -----------------------------------------------------------------------------
Lattice Lattice::LUB(const Lattice &that) const
{
  if (*this == that) {
    return *this;
  }
  if (that.IsUnknown()) {
    return *this;
  }
  if (IsUnknown()) {
    return that;
  }
  if (IsGlobal() && that.IsPointerLike()) {
    if (globalVal_.Sym->IsWeak() && !globalVal_.Off) {
      return Lattice::Overdefined();
    } else {
      return Lattice::Pointer();
    }
  }
  if (IsPointerLike() && that.IsGlobal()) {
    if (that.globalVal_.Sym->IsWeak() && !that.globalVal_.Off) {
      return Lattice::Overdefined();
    } else {
      return Lattice::Pointer();
    }
  }
  if (IsPointerLike() && that.IsPointerLike()) {
    return Lattice::Pointer();
  }
  return Lattice::Overdefined();
}

// -----------------------------------------------------------------------------
Lattice Lattice::Unknown()
{
  return Lattice(Kind::UNKNOWN);
}

// -----------------------------------------------------------------------------
Lattice Lattice::Overdefined()
{
  return Lattice(Kind::OVERDEFINED);
}

// -----------------------------------------------------------------------------
Lattice Lattice::Undefined()
{
  return Lattice(Kind::UNDEFINED);
}

// -----------------------------------------------------------------------------
Lattice Lattice::Pointer()
{
  return Lattice(Kind::POINTER);
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
  return Lattice::CreateInteger(APInt(64, i, true));
}

// -----------------------------------------------------------------------------
Lattice Lattice::CreateInteger(const APInt &i)
{
  Lattice v(Kind::INT);
  new (&v.intVal_) APInt(i);
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
Lattice Lattice::CreateFloatZero()
{
  return Lattice(Kind::FLOAT_ZERO);
}

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, const Lattice &l)
{
  switch (l.GetKind()) {
    case Lattice::Kind::UNKNOWN: {
      OS << "unknown";
      return OS;
    }
    case Lattice::Kind::OVERDEFINED: {
      OS << "overdefined";
      return OS;
    }
    case Lattice::Kind::INT: {
      auto i = l.GetInt();
      OS << "int{" << i << ":" << i.getBitWidth() << "}";
      return OS;
    }
    case Lattice::Kind::FLOAT: {
      llvm::SmallVector<char, 16> buffer;
      l.GetFloat().toString(buffer);
      OS << "float{" << buffer << "}";
      return OS;
    }
    case Lattice::Kind::FLOAT_ZERO: {
      OS << "float{+-0}";
      return OS;
    }
    case Lattice::Kind::FRAME: {
      OS << "frame{" << l.GetFrameObject() << ", " << l.GetFrameOffset() << "}";
      return OS;
    }
    case Lattice::Kind::GLOBAL: {
      const auto &name = l.GetGlobalSymbol()->getName();
      OS << "global{" << name << " + " << l.GetGlobalOffset() << "}";
      return OS;
    }
    case Lattice::Kind::UNDEFINED: {
      OS << "undefined";
      return OS;
    }
    case Lattice::Kind::POINTER: {
      OS << "pointer";
      return OS;
    }
  }
  llvm_unreachable("invalid lattice value kind");
}
