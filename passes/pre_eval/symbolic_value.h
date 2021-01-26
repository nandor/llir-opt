// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/raw_ostream.h>

#include "core/ref.h"
#include "passes/pre_eval/symbolic_pointer.h"

class SymbolicFrame;
class Inst;
using APInt = llvm::APInt;
using APFloat = llvm::APFloat;



/**
 * Representation for a symbolic value.
 */
class SymbolicValue final {
public:
  /// Enumeration of value kinds.
  enum class Kind {
    /// A undefined value.
    UNDEFINED,
    /// A integer of an unknown value.
    SCALAR,
    /// A specific integer.
    INTEGER,
    /// An unknown integer with a lower bound.
    LOWER_BOUNDED_INTEGER,
    /// An integer with some known bits.
    MASKED_INTEGER,
    /// Floating-point value.
    FLOAT,
    /// A pointer or a range of pointers.
    POINTER,
    /// A pointer or null.
    NULLABLE,
    /// Value - unknown integer or pointer.
    VALUE,
  };

  /// Instruction which originated the value.
  using Origin = std::pair<ID<SymbolicFrame>, Ref<Inst>>;

public:
  /// Undefined constructor.
  SymbolicValue() : kind_(Kind::UNDEFINED) {}
  /// Copy constructor.
  SymbolicValue(const SymbolicValue &that);
  /// Cleanup.
  ~SymbolicValue();

  /// Copy assignment operator.
  SymbolicValue &operator=(const SymbolicValue &that);

  static SymbolicValue Scalar(
      const std::optional<Origin> &orig = std::nullopt
  );

  static SymbolicValue Undefined(
      const std::optional<Origin> &orig = std::nullopt
  );

  static SymbolicValue Float(
      const APFloat &val,
      const std::optional<Origin> &orig = std::nullopt
  );

  static SymbolicValue Integer(
      const APInt &val,
      const std::optional<Origin> &orig = std::nullopt
  );

  static SymbolicValue LowerBoundedInteger(
      const APInt &bound,
      const std::optional<Origin> &orig = std::nullopt
  );

  static SymbolicValue Mask(
      const APInt &known,
      const APInt &value,
      const std::optional<Origin> &orig = std::nullopt
  );

  static SymbolicValue Pointer(
      const SymbolicPointer::Ref &pointer,
      const std::optional<Origin> &orig = std::nullopt
  );

  static SymbolicValue Value(
      const SymbolicPointer::Ref &pointer,
      const std::optional<Origin> &orig = std::nullopt
  );

  static SymbolicValue Nullable(
      const SymbolicPointer::Ref &pointer,
      const std::optional<Origin> &orig = std::nullopt
  );

  Kind GetKind() const { return kind_; }

  bool IsInteger() const { return GetKind() == Kind::INTEGER; }
  bool IsScalar() const { return GetKind() == Kind::SCALAR; }
  bool IsLowerBoundedInteger() const { return GetKind() == Kind::LOWER_BOUNDED_INTEGER; }
  bool IsMaskedInteger() const { return GetKind() == Kind::MASKED_INTEGER; }
  bool IsFloat() const { return GetKind() == Kind::FLOAT; }
  bool IsPointer() const { return GetKind() == Kind::POINTER; }
  bool IsValue() const { return GetKind() == Kind::VALUE; }
  bool IsNullable() const { return GetKind() == Kind::NULLABLE; }

  bool IsPointerLike() const { return IsPointer() || IsValue() || IsNullable(); }
  bool IsIntegerLike() const { return IsInteger() || IsLowerBoundedInteger(); }

  APInt GetInteger() const { assert(IsIntegerLike()); return intVal_; }

  APInt GetMaskKnown() const { assert(IsMaskedInteger()); return maskVal_.Known; }
  APInt GetMaskValue() const { assert(IsMaskedInteger()); return maskVal_.Value; }

  APFloat GetFloat() const { assert(IsFloat()); return floatVal_; }

  const SymbolicPointer::Ref &GetPointer() const
  {
    assert(IsPointerLike());
    return ptrVal_;
  }

  const SymbolicPointer *AsPointer() const
  {
    if (IsPointerLike()) {
      return &*GetPointer();
    } else {
      return nullptr;
    }
  }

  std::optional<APInt> AsInt() const
  {
    if (IsInteger()) {
      return std::optional<APInt>(GetInteger());
    } else {
      return std::nullopt;
    }
  }

  /// Pin the value to a different instruction.
  SymbolicValue Pin(Ref<Inst> ref, ID<SymbolicFrame> frame) const;

  /// Checks whether the value evaluates to true.
  bool IsTrue() const;
  /// Checks whether the value evaluates to false.
  bool IsFalse() const;

  /// Return the origin, if there is one.
  std::optional<Origin> GetOrigin() const { return origin_; }

  /// Cast the value to a specific type.
  SymbolicValue Cast(Type type) const;

  /// Merges a value into this one.
  void Merge(const SymbolicValue &that);
  /// Computes the least-upper-bound.
  [[nodiscard]] SymbolicValue LUB(const SymbolicValue &that) const
  {
    SymbolicValue result(*this);
    result.Merge(that);
    return result;
  }

  /// Compares two values for equality.
  bool operator==(const SymbolicValue &that) const;
  bool operator!=(const SymbolicValue &that) const { return !(*this == that); }

  /// Dump the textual representation to a stream.
  void dump(llvm::raw_ostream &os) const;

private:
  /// Constructor which sets the kind.
  SymbolicValue(Kind kind, std::optional<Origin> origin)
    : kind_(kind)
    , origin_(origin)
  {
  }

  /// Cleanup.
  void Destroy();

private:
  /// Kind of the underlying value.
  Kind kind_;
  /// Origin, if known and accurate.
  std::optional<Origin> origin_;
  /// Union of all possible values.
  union {
    /// Value if kind is integer.
    APInt intVal_;
    /// Value if kind is float.
    APFloat floatVal_;
    /// Value is kind is mask.
    struct {
      /// 1's for bits whose values are known.
      APInt Known;
      /// Values of the known bits.
      APInt Value;
    } maskVal_;
    /// Value if kind is pointer.
    std::shared_ptr<SymbolicPointer> ptrVal_;
  };
};

/// Print the value to a stream.
inline llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os,
    const SymbolicValue &sym)
{
  sym.dump(os);
  return os;
}
