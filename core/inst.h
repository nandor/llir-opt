// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>
#include <optional>

#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>

#include "core/constant.h"
#include "core/expr.h"
#include "core/type.h"
#include "core/value.h"

class Block;
class Inst;
class Context;
class Symbol;



/**
 * Condition flag.
 */
enum class Cond {
  EQ, OEQ, UEQ,
  NE, ONE, UNE,
  LT, OLT, ULT,
  GT, OGT, UGT,
  LE, OLE, ULE,
  GE, OGE, UGE,
};


/**
 * Allowed annotations.
 */
enum Annot {
  CAML_CALL_FRAME  = (1 << 0),
  CAML_RAISE_FRAME = (1 << 1),
  CAML_ROOT_FRAME  = (1 << 2),
};


class InvalidPredecessorException : public std::exception {};
class InvalidSuccessorException : public std::exception {};
class InvalidOperandException : public std::exception {};



/**
 * Basic instruction.
 */
class Inst
  : public llvm::ilist_node_with_parent<Inst, Block>
  , public Value
  , public User
{
public:
  /**
   * Enumeration of instruction types.
   */
  enum class Kind : uint8_t {
    // Control flow.
    CALL, TCALL, INVOKE, TINVOKE,RET,
    JCC, JI, JMP, SWITCH, TRAP,
    // Memory.
    LD, ST,
    // Atomic exchange.
    XCHG,
    // Set register.
    SET,
    // Variable argument lists.
    VASTART,
    // Constant.
    ARG, FRAME,
    // Conditional.
    SELECT,
    // Unary instructions.
    ABS, NEG, SQRT, SIN, COS,
    SEXT, ZEXT, FEXT,
    MOV, TRUNC,
    // Binary instructions.
    ADD, AND, CMP, DIV, REM, MUL, OR,
    ROTL, SLL, SRA, SRL, SUB, XOR,
    POW, COPYSIGN,
    // Overflow tests.
    UADDO, UMULO,
    // Undefined value.
    UNDEF,
    // PHI node.
    PHI,
  };

  /// Destroys an instruction.
  virtual ~Inst();

  /// Removes an instruction from the parent.
  void eraseFromParent();

  /// Returns the instruction kind.
  Kind GetKind() const { return kind_; }
  /// Checks if the instruction is of a specific kind.
  bool Is(Kind kind) const { return GetKind() == kind; }
  /// Returns the parent node.
  Block *getParent() const { return parent_; }
  /// Returns the number of returned values.
  virtual unsigned GetNumRets() const = 0;
  /// Returns the type of the ith return value.
  virtual Type GetType(unsigned i) const = 0;

  /// Returns the size of the instruction.
  virtual std::optional<size_t> GetSize() const { return std::nullopt; }
  /// Checks if the instruction is a terminator.
  virtual bool IsTerminator() const { return false; }

  /// Checks if a flag is set.
  bool HasAnnotation(Annot annot) const { return (annot_ & annot) != 0; }
  /// Checks if any flags are set.
  bool IsAnnotated() const { return annot_ != 0; }

protected:
  /// Constructs an instruction of a given type.
  Inst(Kind kind, Block *parent, unsigned numOps, uint64_t annot = 0)
    : Value(Value::Kind::INST)
    , User(numOps)
    , kind_(kind)
    , parent_(parent)
    , annot_(annot)
  {
    assert(parent != nullptr && "invalid parent");
  }

private:
  /// Instruction kind.
  const Kind kind_;
  /// Instruction annotation.
  const uint64_t annot_;

protected:
  /// Parent node.
  Block *parent_;
};


class ControlInst : public Inst {
public:
  /// Constructs a control flow instructions.
  ControlInst(Kind kind, Block *parent, unsigned numOps, uint64_t annot = 0)
    : Inst(kind, parent, numOps, annot)
  {
  }
};

class TerminatorInst : public ControlInst {
public:
  /// Constructs a terminator instruction.
  TerminatorInst(Kind kind, Block *parent, unsigned numOps, uint64_t annot = 0)
    : ControlInst(kind, parent, numOps, annot)
  {
  }

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
  /// Constructs a memory instruction.
  MemoryInst(Kind kind, Block *parent, unsigned numOps)
    : Inst(kind, parent, numOps)
  {
  }
};

class StackInst : public MemoryInst {
public:
  /// Constructs a stack instruction.
  StackInst(Kind kind, Block *parent, unsigned numOps)
    : MemoryInst(kind, parent, numOps)
  {
  }
};

/**
 * Instruction with a single typed return.
 */
class OperatorInst : public Inst {
public:
  /// Constructs an instruction.
  OperatorInst(Kind kind, Block *parent, Type type, unsigned numOps)
    : Inst(kind, parent, numOps)
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

/**
 * Instruction with a constant operand.
 */
class ConstInst : public OperatorInst {
public:
  /// Constructs a constant instruction.
  ConstInst(Kind kind, Block *parent, Type type, unsigned numOps)
    : OperatorInst(kind, parent, type, numOps)
  {
  }
};

/*
 * Instruction with a unary operand.
 */
class UnaryInst : public OperatorInst {
public:
  /// Constructs a unary operator instruction.
  UnaryInst(
      Kind kind,
      Block *parent,
      Type type,
      Inst *arg
  );

  /// Returns the sole argument.
  Inst *GetArg() const;
};

/**
 * Instructions with two operands.
 */
class BinaryInst : public OperatorInst {
public:
  /// Constructs a binary operator instruction.
  BinaryInst(
      Kind kind,
      Block *parent,
      Type type,
      Inst *lhs,
      Inst *rhs
  );

  /// Returns the LHS operator.
  Inst *GetLHS() const;
  /// Returns the RHS operator.
  Inst *GetRHS() const;
};

/**
 * Overflow-checking instructions.
 */
class OverflowInst : public BinaryInst {
public:
  /// Constructs an overflow-checking instruction.
  OverflowInst(Kind kind, Block *parent, Inst *lhs, Inst *rhs)
    : BinaryInst(kind, parent, Type::I32, lhs, rhs)
  {
  }
};
