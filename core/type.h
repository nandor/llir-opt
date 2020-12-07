// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MachineValueType.h>
#include <llvm/Support/Alignment.h>



/**
 * Data Types known to the IR.
 */
enum class Type {
  I8,
  I16,
  I32,
  I64,
  V64,
  I128,
  F32,
  F64,
  F80,
  F128
};

/**
 * Additional annotations attached to a value.
 */
class TypeFlag {
public:
  enum class Kind : uint8_t {
    NONE,
    BYVAL,
    SEXT,
    ZEXT,
  };

public:
  static TypeFlag getNone();
  static TypeFlag getSExt();
  static TypeFlag getZExt();
  static TypeFlag getByVal(unsigned size, llvm::Align align);

  Kind GetKind() const { return static_cast<Kind>(kind_); }

  bool operator==(const TypeFlag &that) const
  {
    return true;
  }

  unsigned GetByValSize() const;
  llvm::Align GetByValAlign() const;

private:
  /// Prevent type flags from being built.
  TypeFlag() { }

private:
  union {
    uint64_t data_;
    struct {
      unsigned size_ : 16;
      unsigned align_ : 16;
      unsigned kind_ : 8;
    };
  };
};

/**
 * Type with a flag attached to it.
 */
class FlaggedType {
public:
  FlaggedType(Type type) : type_(type), flag_(TypeFlag::getNone()) {}
  FlaggedType(Type type, TypeFlag flag) : type_(type), flag_(flag) {}

  Type GetType() const { return type_; }
  TypeFlag GetFlag() const { return flag_; }

  bool operator==(const FlaggedType &that) const
  {
    return type_ == that.type_ && flag_ == that.flag_;
  }

private:
  /// Underlying type.
  Type type_;
  /// Flag.
  TypeFlag flag_;
};

/**
 * Prints a type to a stream.
 */
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Type ty);

/**
 * Checks if the type is an integer type.
 */
bool IsIntegerType(Type type);

/**
 * Checks if the type is a pointer type.
 */
bool IsPointerType(Type type);

/**
 * Checks if the type is a floating point type.
 */
bool IsFloatType(Type type);

/**
 * Returns the size of a type in bytes.
 */
unsigned GetSize(Type type);

/**
 * Returns the alignment of the type in bytes.
 */
llvm::Align GetAlignment(Type type);

/**
 * Returns the equivalent LLVM MachineValueType.
 */
llvm::MVT GetVT(Type type);
