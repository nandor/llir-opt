// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>

#include "core/value.h"
#include "core/register.h"

using APInt = llvm::APInt;
using APFloat = llvm::APFloat;



/**
 * Base class of non-mutable values.
 */
class Constant : public Value {
public:
  /// Kind of the global.
  static constexpr Value::Kind kValueKind = Value::Kind::CONST;

public:
  /**
   * Enumeration of constant kinds.
   */
  enum Kind {
    INT,
    FLOAT,
    REG
  };

  Constant(Kind kind) : Value(Value::Kind::CONST), kind_(kind) {}

  virtual ~Constant();

  Kind GetKind() const { return kind_; }

  bool Is(Kind kind) const { return GetKind() == kind; }

private:
  /// Returns the kind of the constant.
  Kind kind_;
};


/**
 * Constant integer.
 */
class ConstantInt final : public Constant {
public:
  /// Kind of the constant.
  static constexpr Constant::Kind kConstKind = Constant::Kind::INT;

public:
  ConstantInt(int64_t v);
  ConstantInt(const APInt &v) : Constant(Constant::Kind::INT), v_(v) {}

  APInt GetValue() const { return v_; }
  int64_t GetInt() const { return v_.getSExtValue(); }

private:
  APInt v_;
};


/**
 * Constant float.
 */
class ConstantFloat final : public Constant {
public:
  /// Kind of the constant.
  static constexpr Constant::Kind kConstKind = Constant::Kind::FLOAT;

public:
  ConstantFloat(double d) : Constant(Constant::Kind::FLOAT), v_(APFloat(d)) {}
  ConstantFloat(const APFloat &v) : Constant(Constant::Kind::FLOAT), v_(v) {}

  APFloat GetValue() const { return v_; }
  double GetDouble() const;

private:
  APFloat v_;
};

/**
 * Register reference.
 */
class ConstantReg final : public Constant {
public:
  /// Kind of the constant.
  static constexpr Constant::Kind kConstKind = Constant::Kind::REG;

  /// Enumeration of hardware registers.
  using Kind = Register;

  ConstantReg(Kind kind) : Constant(Constant::Kind::REG), kind_(kind) {}

  Kind GetValue() const { return kind_; }

private:
  Kind kind_;
};
