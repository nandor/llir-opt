// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>
#include <vector>
#include <optional>

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
class Inst {
public:
  /**
   * Enumeration of instruction types.
   */
  enum class Kind {
    // Control flow.
    CALL,  TCALL, JT, JF, JI, JMP, RET,SWITCH,
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
  };

  /// Destroys an instruction.
  virtual ~Inst();

private:
  /// Block holding the instruction.
  Block *block_;
  /// Previous instruction in the block.
  Inst *block_prev_;
  /// Next instruction in the block.
  Inst *block_next_;
};


class FlowInst : public Inst {
public:
};

class TerminatorInst : public FlowInst {
public:
};

class MemoryInst : public Inst {
public:
};

class StackInst : public MemoryInst {
public:
};

class AtomicInst : public Inst {
public:
};

class ConstInst : public Inst {
public:
};

class OperatorInst : public Inst {
public:
};

class UnaryOperatorInst : public OperatorInst {
public:
  UnaryOperatorInst(Type type, const Operand &lhs)
  {
  }
};

class BinaryOperatorInst : public OperatorInst {
public:
  BinaryOperatorInst(Type type, const Operand &lhs, const Operand &rhs)
  {
  }
};
