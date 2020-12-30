// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_set>
#include <unordered_map>

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/raw_ostream.h>

using APInt = llvm::APInt;
using APFloat = llvm::APFloat;

class Global;



/**
 * An address or a range of addresses.
 */
class SymbolicPointer final {
public:
  using pointer_iterator = std::unordered_map<Global *, int64_t>::const_iterator;
  using range_iterator = std::unordered_set<Global *>::const_iterator;

public:
  SymbolicPointer();
  SymbolicPointer(Global *symbol, int64_t offset);
  SymbolicPointer(const SymbolicPointer &that);
  SymbolicPointer(SymbolicPointer &&that);
  ~SymbolicPointer();

  /// Convert the pointer to a precise one, if it is one.
  std::optional<std::pair<Global *, int64_t>> ToPrecise() const;

  /// Compares two sets of pointers for equality.
  bool operator==(const SymbolicPointer &that) const;

  /// Offset the pointer.
  SymbolicPointer Offset(int64_t offset) const;

  /// Computes the least-upper-bound.
  SymbolicPointer LUB(const SymbolicPointer &that) const;

  /// Dump the textual representation to a stream.
  void dump(llvm::raw_ostream &os) const;

  /// Iterator over pointer.
  pointer_iterator pointer_begin() const { return pointers_.begin(); }
  pointer_iterator pointer_end() const { return pointers_.end(); }
  llvm::iterator_range<pointer_iterator> pointers() const
  {
    return llvm::make_range(pointer_begin(), pointer_end());
  }

  /// Iterator over ranges.
  range_iterator range_begin() const { return ranges_.begin(); }
  range_iterator range_end() const { return ranges_.end(); }
  llvm::iterator_range<range_iterator> ranges() const
  {
    return llvm::make_range(range_begin(), range_end());
  }

private:
  /// Set of direct global pointees.
  std::unordered_map<Global *, int64_t> pointers_;
  /// Set of imprecise global ranges.
  std::unordered_set<Global *> ranges_;
};

/// Print the pointer to a stream.
inline llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os,
    const SymbolicPointer &sym)
{
  sym.dump(os);
  return os;
}



/**
 * Representation for a symbolic value.
 */
class SymbolicValue final {
public:
  enum class Kind {
    /// An arbitrary value type.
    UNKNOWN,
    /// A integer of an unknown value.
    UNKNOWN_INTEGER,
    /// A specific integer.
    INTEGER,
    /// A pointer or a range of pointers.
    POINTER,
  };

public:
  /// Copy constructor.
  SymbolicValue(const SymbolicValue &that);
  /// Cleanup.
  ~SymbolicValue();

  /// Copy assignment operator.
  SymbolicValue &operator=(const SymbolicValue &that);

  static SymbolicValue Unknown();
  static SymbolicValue UnknownInteger();
  static SymbolicValue Integer(const APInt &val);
  static SymbolicValue Address(Global *symbol, int64_t offset);
  static SymbolicValue Pointer(SymbolicPointer &&pointer);

  Kind GetKind() const { return kind_; }

  bool IsUnknown() const { return GetKind() == Kind::UNKNOWN; }
  bool IsInteger() const { return GetKind() == Kind::INTEGER; }
  bool IsPointer() const { return GetKind() == Kind::POINTER; }

  APInt GetInteger() const { assert(IsInteger()); return intVal_; }
  std::optional<APInt> AsInt() const
  {
    if (IsInteger()) {
      return std::optional<APInt>(GetInteger());
    } else {
      return std::nullopt;
    }
  }

  SymbolicPointer GetPointer() const { assert(IsPointer()); return ptrVal_; }
  std::optional<SymbolicPointer> AsPointer()
  {
    if (IsPointer()) {
      return std::optional<SymbolicPointer>(GetPointer());
    } else {
      return std::nullopt;
    }
  }

  /// Computes the least-upper-bound.
  SymbolicValue LUB(const SymbolicValue &that) const;

  /// Compares two values for equality.
  bool operator==(const SymbolicValue &that) const;
  bool operator!=(const SymbolicValue &that) const { return !(*this == that); }

  /// Dump the textual representation to a stream.
  void dump(llvm::raw_ostream &os) const;

private:
  /// Constructor which sets the kind.
  SymbolicValue(Kind kind) : kind_(kind) {}

  /// Cleanup.
  void Destroy();

private:
  /// Kind of the underlying value.
  Kind kind_;
  /// Union of all possible values.
  union {
    /// Value if kind is integer.
    APInt intVal_;
    /// Value if kind is pointer.
    SymbolicPointer ptrVal_;
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
