// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>
#include <vector>
#include <optional>

#include <llvm/ADT/ilist_node.h>
#include "llvm/ADT/ilist.h"

#include "core/expr.h"
#include "core/value.h"
#include "core/operand.h"

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
  LT, OLT, ULT,
  GT, OGT, UGT,
  LE, OLE, ULE,
  GE, OGE, UGE,
};


/**
 * Exception thrown when the operand is out of bounds.
 */
class InvalidOperandException : public std::exception {
};

/**
 * Exception thrown when a successor does not exist.
 */
class InvalidSuccessorException : public std::exception {
};

/**
 * Exception thrown when a PHI node does not have an incoming block.
 */
class InvalidPredecessorException : public std::exception {
};

/**
 * Basic instruction.
 */
class Inst : public llvm::ilist_node_with_parent<Inst, Block>, public User {
public:
  /**
   * Enumeration of instruction types.
   */
  enum class Kind : uint8_t {
    // Control flow.
    CALL, TCALL, INVOKE, RET,
    JCC, JI, JMP, SWITCH, TRAP,
    // Memory.
    LD, ST, PUSH, POP,
    // Atomic exchange.
    XCHG,
    // Set register.
    SET,
    // Constant.
    IMM, ADDR, ARG,
    // Conditional.
    SELECT,
    // Unary instructions.
    ABS, MOV, NEG, SEXT, ZEXT, TRUNC,
    // Binary instructions.
    ADD, AND, CMP, DIV, REM, MUL, OR,
    ROTL, SLL, SRA, SRL, SUB, XOR,
    // PHI node.
    PHI
  };

  /// Destroys an instruction.
  virtual ~Inst();

  /// Returns the instruction kind.
  Kind GetKind() const { return kind_; }
  /// Checks if the instruction is of a specific kind.
  bool Is(Kind kind) const { return GetKind() == kind; }
  /// Returns the parent node.
  Block *GetParent() const { return parent_; }
  /// Returns the number of operands.
  virtual unsigned GetNumOps() const = 0;
  /// Returns the number of returned values.
  virtual unsigned GetNumRets() const = 0;
  /// Returns the type of the ith return value.
  virtual Type GetType(unsigned i) const = 0;
  /// Returns an operand.
  virtual const Operand &GetOp(unsigned i) const = 0;
  /// Sets an operand.
  virtual void SetOp(unsigned i, const Operand &op) = 0;

  /// Returns the size of the instruction.
  virtual std::optional<size_t> GetSize() const { return std::nullopt; }
  /// Checks if the instruction is a terminator.
  virtual bool IsTerminator() const { return false; }

protected:
  /// Constructs an instruction of a given type.
  Inst(Kind kind, Block *parent)
    : User(Value::Kind::INST)
    , kind_(kind)
    , parent_(parent)
  {
    assert(parent != nullptr && "invalid parent");
  }

private:
  /// Instruction kind.
  Kind kind_;

protected:
  /// Parent node.
  Block *parent_;
};


class ControlInst : public Inst {
public:
  /// Constructs a control flow instructions.
  ControlInst(Kind kind, Block *parent) : Inst(kind, parent) {}
};

class TerminatorInst : public ControlInst {
public:
  /// Constructs a terminator instruction.
  TerminatorInst(Kind kind, Block *parent) : ControlInst(kind, parent) {}

  /// Terminators do not return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Checks if the instruction is a terminator.
  bool IsTerminator() const override { return true; }
  /// Returns the number of successors.
  virtual unsigned getNumSuccessors() const = 0;
  /// Returns a successor.
  virtual Block *getSuccessor(unsigned idx) const = 0;

};

class MemoryInst : public Inst {
public:
  /// Constructs a terminator instruction.
  MemoryInst(Kind kind, Block *parent) : Inst(kind, parent) {}
};

class StackInst : public MemoryInst {
public:
  /// Constructs a terminator instruction.
  StackInst(Kind kind, Block *parent) : MemoryInst(kind, parent) {}
};

class AtomicInst : public Inst {
public:
  /// Constructs a terminator instruction.
  AtomicInst(Kind kind, Block *parent) : Inst(kind, parent) {}
};

class ConstInst : public Inst {
public:
  /// Constructs a terminator instruction.
  ConstInst(Kind kind, Block *parent) : Inst(kind, parent) {}
};

class OperatorInst : public Inst {
public:
  /// Constructs a terminator instruction.
  OperatorInst(Kind kind, Block *parent, Type type)
    : Inst(kind, parent)
    , type_(type)
  {
  }

  /// Unary operators return a single value.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the type of the instruction.
  Type GetType() const { return type_; }

private:
  /// Return value type.
  Type type_;
};

class UnaryInst : public OperatorInst {
public:
  /// Constructs a unary operator instruction.
  UnaryInst(
      Kind kind,
      Block *parent,
      Type type,
      const Operand &arg)
    : OperatorInst(kind, parent, type)
    , arg_(arg)
  {
  }

  /// Unary operators have a single operand.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Unary operator operand.
  Operand arg_;
};

class BinaryInst : public OperatorInst {
public:
  /// Constructs a binary operator instruction.
  BinaryInst(
      Kind kind,
      Block *parent,
      Type type,
      const Operand &lhs,
      const Operand &rhs)
    : OperatorInst(kind, parent, type)
    , lhs_(lhs)
    , rhs_(rhs)
  {
  }

  /// Binary operators have two operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the LHS operator.
  Inst *GetLHS() const { return lhs_.GetInst(); }
  /// Returns the RHS operator.
  Inst *GetRHS() const { return rhs_.GetInst(); }

private:
  /// LHS operand.
  Operand lhs_;
  /// RHS operand.
  Operand rhs_;
};
