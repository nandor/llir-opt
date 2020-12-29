// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_set>

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/raw_ostream.h>

using APInt = llvm::APInt;
using APFloat = llvm::APFloat;

class Global;



/**
 * A single address.
 */
class SymbolicAddress final {
public:
  /// Enumeration of address kinds.
  enum class Kind : uint8_t {
    GLOBAL,
  };

public:
  /// Construct an address to a specific location.
  SymbolicAddress(Global *symbol, int64_t offset) : v_(symbol, offset) {}

  /// Convert the global to an address.
  std::optional<std::pair<Global *, int64_t>> ToGlobal() const;

  /// Compares two addresses for equality.
  bool operator==(const SymbolicAddress &that) const;

  /// Hasher for the type.
  struct Hash {
    size_t operator()(const SymbolicAddress &that) const;
  };

  /// Prints the address.
  void dump(llvm::raw_ostream &os) const;

private:
  /// Exact global address.
  struct AddrGlobal {
    /// kind of the symbol.
    Kind K;
    /// Global symbol.
    Global *Symbol;
    /// Offset into the symbol.
    int64_t Offset;

    AddrGlobal(Global *symbol, int64_t offset)
        : K(Kind::GLOBAL), Symbol(symbol), Offset(offset)
    {
    }
  };

  /// Range of an entire object.
  struct AddrRange {

  };

  /// Base symbol.
  union S {
    /// kind of the symbol.
    Kind K;
    /// Global address.
    AddrGlobal G;
    /// Range.
    AddrRange R;

    /// Constructs storage pointing to a global.
    S(Global *symbol, int64_t offset) : G(symbol, offset) {}

    /// Compares the storage for equality.
    bool operator==(const S &that) const;
  } v_;

  /// Hasher for the global address.
  struct AddrGlobalHash {
    size_t operator()(const AddrGlobal &that) const;
  };

  /// Hasher for the global address.
  struct AddrRangeHash {
    size_t operator()(const AddrRange &that) const;
  };
};

/// Print the value to a stream.
inline llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os,
    const SymbolicAddress &sym)
{
  sym.dump(os);
  return os;
}



/**
 * An address or a range of addresses.
 */
class SymbolicPointer final {
public:
  SymbolicPointer(Global *symbol, int64_t offset);
  SymbolicPointer(const SymbolicPointer &that);
  ~SymbolicPointer();

  /// Convert the pointer to a precise one, if it is one.
  std::optional<std::pair<Global *, int64_t>> ToPrecise() const;

  /// Compares two sets of pointers for equality.
  bool operator==(const SymbolicPointer &that) const
  {
    return addresses_ == that.addresses_;
  }

  /// Dump the textual representation to a stream.
  void dump(llvm::raw_ostream &os) const;

private:
  /// Set of pointees.
  std::unordered_set<SymbolicAddress, SymbolicAddress::Hash> addresses_;
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
    UNKNOWN,
    INTEGER,
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
  static SymbolicValue Integer(const APInt &val);
  static SymbolicValue Address(Global *symbol, int64_t offset);

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
