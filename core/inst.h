// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include "adt/chain.h"

class Block;
class Inst;
class Context;
class Symbol;



/**
 * Data Types known to the IR.
 */
enum class Type {
  I8, I16, I32, I64,
  U8, U16, U32, U64,
  F32, F64
};

/**
 * Condition flag.
 */
enum class Cond {
  EQ, NEQ,
  LT, NLT,
  GT, NGT,
  LE, NLE,
  GE, NGE,
};

/**
 * Registers.
 */
enum class Reg {
  SP,
  FP,
};


/**
 * Expression operand.
 */
class Expr {
public:
  static Expr *CreateSymbolOff(Context &ctx, Symbol *sym, int64_t offset);
};

/**
 * Operand to an instruction.
 */
class Operand {
public:
  Operand(int64_t intVal) : type_(Type::INT), intVal_(intVal) { }
  Operand(float floatVal) : type_(Type::FLOAT), floatVal_(floatVal) { }
  Operand(Reg regVal) : type_(Type::REG), regVal_(regVal) { }
  Operand(Inst *instVal) : type_(Type::INST), instVal_(instVal) { }
  Operand(Symbol *symVal) : type_(Type::SYM), symVal_(symVal) { }
  Operand(Expr *exprVal) : type_(Type::EXPR), exprVal_(exprVal) { }
  Operand(Block *blockVal) : type_(Type::BLOCK), blockVal_(blockVal) { }

  bool IsInt() const { return type_ == Type::INT; }
  bool IsBlock() const { return type_ == Type::BLOCK; }

  int64_t GetImm() const { assert(IsInt()); return intVal_; }
  Block *GetBlock() const { assert(IsBlock()); return blockVal_; }

private:
  enum class Type {
    INT,
    FLOAT,
    REG,
    INST,
    SYM,
    EXPR,
    BLOCK,
  };

  Type type_;

  union {
    int64_t intVal_;
    double floatVal_;
    Reg regVal_;
    Inst *instVal_;
    Symbol *symVal_;
    Expr *exprVal_;
    Block *blockVal_;
  };
};


/**
 * Basic instruction.
 */
class Inst : public ChainNode<Inst> {
public:
  /**
   * Enumeration of instruction types.
   */
  enum class Kind : uint8_t {
    // Control flow.
    CALL, TCALL, JT, JF, JI, JMP, RET, SWITCH,
    // Memory.
    LD, ST, PUSH, POP,
    // Atomic exchange.
    XCHG,
    // Constant.
    IMM, ADDR, ARG,
    // Conditional.
    SELECT,
    // Unary instructions.
    ABS, MOV, NEG, SEXT, ZEXT, TRUNC,
    // Binary instructions.
    ADD, AND, ASR, CMP, DIV, LSL, LSR, MOD, MUL,
    MULH, OR, ROTL, SHL, SRA, REM, SRL, SUB, XOR,
    // PHI node.
    PHI
  };

  /// Destroys an instruction.
  virtual ~Inst();

  /// Returns the instruction kind.
  Kind GetKind() const { return kind_; }
  /// Returns the number of operands.
  unsigned getNumOperands() const { return 0; }

protected:
  /// Constructs an instruction of a given type.
  Inst(Kind kind) : kind_(kind) {}

private:
  /// Instruction kind.
  Kind kind_;
};


class ControlInst : public Inst {
public:
  /// Constructs a control flow instructions.
  ControlInst(Kind kind) : Inst(kind) {}
};

class TerminatorInst : public ControlInst {
public:
  /// Constructs a terminator instruction.
  TerminatorInst(Kind kind) : ControlInst(kind) {}
};

class MemoryInst : public Inst {
public:
  /// Constructs a terminator instruction.
  MemoryInst(Kind kind) : Inst(kind) {}
};

class StackInst : public MemoryInst {
public:
  /// Constructs a terminator instruction.
  StackInst(Kind kind) : MemoryInst(kind) {}
};

class AtomicInst : public Inst {
public:
  /// Constructs a terminator instruction.
  AtomicInst(Kind kind) : Inst(kind) {}
};

class ConstInst : public Inst {
public:
  /// Constructs a terminator instruction.
  ConstInst(Kind kind) : Inst(kind) {}
};

class OperatorInst : public Inst {
public:
  /// Constructs a terminator instruction.
  OperatorInst(Kind kind) : Inst(kind) {}
};

class UnaryOperatorInst : public OperatorInst {
public:
  /// Constructs a unary operator instruction.
  UnaryOperatorInst(
      Kind kind,
      Type type,
      const Operand &lhs)
    : OperatorInst(kind)
  {
  }
};

class BinaryOperatorInst : public OperatorInst {
public:
  /// Constructs a binary operator instruction.
  BinaryOperatorInst(
      Kind kind,
      Type type,
      const Operand &lhs,
      const Operand &rhs)
    : OperatorInst(kind)
  {
  }
};
