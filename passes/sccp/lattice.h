// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <optional>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/raw_ostream.h>

using APInt = llvm::APInt;
using APFloat = llvm::APFloat;



/**
 * Lattice for SCCP values.
 */
class Lattice {
public:
  /// Enumeration of lattice value kinds.
  enum class Kind {
    /// Top - value not encountered yet.
    UNKNOWN,
    /// Bot - value is not constant.
    OVERDEFINED,
    /// Constant integer.
    INT,
    /// Constant floating-point.
    FLOAT,
    /// Offset into the frame.
    FRAME,
    /// Constant symbol with a potential offset.
    GLOBAL,
    /// Constant, undefined.
    UNDEFINED,
  };

public:
  Lattice(const Lattice &that);
  ~Lattice();

  Kind GetKind() const { return kind_; }
  bool IsUnknown() const { return GetKind() == Kind::UNKNOWN; }
  bool IsOverdefined() const { return GetKind() == Kind::OVERDEFINED; }
  bool IsUndefined() const { return GetKind() == Kind::UNDEFINED; }
  bool IsInt() const { return GetKind() == Kind::INT; }
  bool IsFloat() const { return GetKind() == Kind::FLOAT; }
  bool IsGlobal() const { return GetKind() == Kind::GLOBAL; }
  bool IsFrame() const { return GetKind() == Kind::FRAME; }

  APInt GetInt() const { assert(IsInt()); return intVal_; }
  APFloat GetFloat() const { assert(IsFloat()); return floatVal_; }
  unsigned GetFrameObject() const { assert(IsFrame()); return frameVal_.Obj; }
  int64_t GetFrameOffset() const { assert(IsFrame()); return frameVal_.Off; }
  Global *GetGlobalSymbol() const { assert(IsGlobal()); return globalVal_.Sym; }
  int64_t GetGlobalOffset() const { assert(IsGlobal()); return globalVal_.Off; }

  bool IsTrue() const;
  bool IsFalse() const;

  /// Returns some integer, if the value is one.
  std::optional<APInt> AsInt() const
  {
    return IsInt() ? std::optional<APInt>(intVal_) : std::nullopt;
  }

  /// Returns some float, if the value is one.
  std::optional<APFloat> AsFloat() const
  {
    return IsFloat() ? std::optional<APFloat>(floatVal_) : std::nullopt;
  }

  /// Checks if two values are not identical.
  bool operator != (const Lattice &that) const { return !(*this == that); }

  /// Checks if two values are identical.
  bool operator == (const Lattice &that) const;

  /// Assigns a value to a lattice.
  Lattice &operator = (const Lattice &that);

  /// Least upper bound operator.
  Lattice LUB(const Lattice &that) const;

public:
  /// Creates an unknown value.
  static Lattice Unknown();
  /// Creates an overdefined value.
  static Lattice Overdefined();
  /// Creates an undefined value.
  static Lattice Undefined();
  /// Creates a frame value.
  static Lattice CreateFrame(unsigned obj, int64_t off);
  /// Creates a global value.
  static Lattice CreateGlobal(Global *g, int64_t Off = 0);
  /// Creates an integral value from an integer.
  static Lattice CreateInteger(int64_t i);
  /// Creates an integral value.
  static Lattice CreateInteger(const APInt &i);
  /// Creates a floating value from a double.
  static Lattice CreateFloat(double f);
  /// Creates a floating value.
  static Lattice CreateFloat(const APFloat &f);

private:
  /// Creates a value of a certain kind.
  Lattice(Kind kind) : kind_(kind) {}
  /// Kind of the lattice value.
  Kind kind_;

  /// Union of possible values.
  union {
    /// Integer value.
    APInt intVal_;
    /// Double value.
    APFloat floatVal_;
    /// Frame value.
    struct {
      /// Object identifier.
      unsigned Obj;
      /// Relative offset.
      int64_t Off;
    } frameVal_;
    /// Global value.
    struct {
      /// Base pointer.
      Global *Sym;
      /// Relative offset.
      int64_t Off;
    } globalVal_;
  };
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, const Lattice &l);
