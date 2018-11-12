// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/value.h"



/**
 * Base class of non-mutable values.
 */
class Constant : public Value {
public:
  /**
   * Enumeration of constant kinds.
   */
  enum Kind {
    INT,
    FLOAT,
    REG,
    UNDEF
  };

  Constant(Kind kind) : Value(Value::Kind::CONST), kind_(kind) {}

  Kind GetKind() const { return kind_; }

private:
  /// Returns the kind of the constant.
  Kind kind_;
};


/**
 * Constant integer.
 */
class ConstantInt final : public Constant {
public:
  ConstantInt(int64_t v) : Constant(Constant::Kind::INT), v_(v) {}

  int64_t GetValue() const { return v_; }

private:
  int64_t v_;
};


/**
 * Constant float.
 */
class ConstantFloat final : public Constant {
public:
  ConstantFloat(double v) : Constant(Constant::Kind::FLOAT), v_(v) {}

  double GetValue() const { return v_; }

private:
  double v_;
};


/**
 * Register reference.
 */
class ConstantReg final : public Constant {
public:
  enum class Kind {
    SP,
    FP,
  };

  ConstantReg(Kind kind) : Constant(Constant::Kind::REG), kind_(kind) {}

  Kind GetValue() const { return kind_; }

private:
  Kind kind_;
};


/**
 * Undefined value.
 */
class ConstantUndef final : public Constant {
public:
  ConstantUndef() : Constant(Constant::Kind::UNDEF) {}
};
