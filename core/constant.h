// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include "core/value.h"

using APInt = llvm::APInt;
using APFloat = llvm::APFloat;



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
    REG
  };

  Constant(Kind kind) : Value(Value::Kind::CONST), kind_(kind) {}

  virtual ~Constant();

  Kind GetKind() const { return kind_; }

  bool Is(Kind kind) { return GetKind() == kind; }

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
  enum class Kind : uint8_t {
    /// X86 Architectural registers.
    RAX, RBX, RCX, RDX, RSI, RDI, RSP, RBP,
    R8, R9, R10, R11, R12, R13, R14, R15,
    FS,
    /// Virtual register taking the value of the return address.
    RET_ADDR,
    /// Virtual register taking the value of the top of the stack.
    FRAME_ADDR,
    // Current program counter.
    PC,
  };

  ConstantReg(Kind kind) : Constant(Constant::Kind::REG), kind_(kind) {}

  Kind GetValue() const { return kind_; }

private:
  Kind kind_;
};
