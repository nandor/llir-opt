// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>
#include <optional>

#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>

#include "core/annot.h"
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

class InvalidPredecessorException : public std::exception {};
class InvalidSuccessorException : public std::exception {};
class InvalidOperandException : public std::exception {};



/**
 * Basic instruction.
 */
class Inst
  : public llvm::ilist_node_with_parent<Inst, Block>
  , public User
{
public:
  /// Kind of the instruction.
  static constexpr Value::Kind kValueKind = Value::Kind::INST;

public:
  /**
   * Enumeration of instruction types.
   */
  enum class Kind : uint8_t {
    // Control flow.
    CALL, TCALL, INVOKE, TINVOKE, RET,
    JCC, JI, JMP, SWITCH, TRAP,
    // Memory.
    LD, ST,
    // Atomic exchange.
    XCHG,
    // Set register.
    SET,
    // Variable argument lists.
    VASTART,
    // Dynamic stack allcoation.
    ALLOCA,
    // Constants.
    ARG, FRAME, UNDEF,
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
    // PHI node.
    PHI,
  };

  /// Destroys an instruction.
  virtual ~Inst();

  /// Removes an instruction from the parent.
  void removeFromParent();
  /// Removes an instruction from the parent and deletes it.
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

  /// Checks if the instruction is void.
  bool IsVoid() const { return GetNumRets() == 0; }

  /// Checks if the instruction is constant.
  virtual bool IsConstant() const = 0;

  /// Returns the size of the instruction.
  virtual std::optional<size_t> GetSize() const { return std::nullopt; }
  /// Checks if the instruction is a terminator.
  virtual bool IsTerminator() const { return false; }

  /// Checks if a flag is set.
  bool HasAnnot(Annot annot) const { return annot_.Has(annot); }
  /// Returns the instruction's annotation.
  AnnotSet GetAnnot() const { return annot_; }
  /// Sets an annotation.
  void SetAnnot(Annot annot) { annot_.Set(annot); }
  /// Removes an annotation.
  void ClearAnnot(Annot annot) { annot_.Clear(annot); }

  /// Checks if any flags are set.
  bool annot_empty() const { return annot_.empty(); }
  /// Iterator over annotations.
  llvm::iterator_range<AnnotSet::iterator> annots() const
  {
    return llvm::make_range(annot_.begin(), annot_.end());
  }


  /// Checks if the instruction has side effects.
  virtual bool HasSideEffects() const = 0;

protected:
  /// Constructs an instruction of a given type.
  Inst(Kind kind, unsigned numOps, const AnnotSet &annot)
    : User(Value::Kind::INST, numOps)
    , kind_(kind)
    , parent_(nullptr)
    , annot_(annot)
  {
  }

private:
  friend struct llvm::ilist_traits<Inst>;
  /// Updates the parent node.
  void setParent(Block *parent) { parent_ = parent; }

private:
  /// Instruction kind.
  const Kind kind_;
  /// Instruction annotation.
  AnnotSet annot_;

protected:
  /// Parent node.
  Block *parent_;
};


class ControlInst : public Inst {
public:
  /// Constructs a control flow instructions.
  ControlInst(Kind kind, unsigned numOps, const AnnotSet &annot)
    : Inst(kind, numOps, annot)
  {
  }

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

class TerminatorInst : public ControlInst {
public:
  /// Constructs a terminator instruction.
  TerminatorInst(Kind kind, unsigned numOps, const AnnotSet &annot)
    : ControlInst(kind, numOps, annot)
  {
  }

  /// Terminators do not return values.
  virtual unsigned GetNumRets() const override;
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
  MemoryInst(Kind kind, unsigned numOps, const AnnotSet &annot)
    : Inst(kind, numOps, annot)
  {
  }

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

class StackInst : public MemoryInst {
public:
  /// Constructs a stack instruction.
  StackInst(Kind kind, unsigned numOps, const AnnotSet &annot)
    : MemoryInst(kind, numOps, annot)
  {
  }
};

/**
 * Instruction with a single typed return.
 */
class OperatorInst : public Inst {
public:
  /// Constructs an instruction.
  OperatorInst(Kind kind, Type type, unsigned numOps, const AnnotSet &annot)
    : Inst(kind, numOps, annot)
    , type_(type)
  {
  }

  /// Unary operators return a single value.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the type of the instruction.
  Type GetType() const { return type_; }

  /// These instructions have no side effects.
  bool HasSideEffects() const override { return false; }

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
  ConstInst(Kind kind, Type type, unsigned numOps, const AnnotSet &annot)
    : OperatorInst(kind, type, numOps, annot)
  {
  }

  /// Instruction is constant.
  bool IsConstant() const override { return true; }
};

/*
 * Instruction with a unary operand.
 */
class UnaryInst : public OperatorInst {
public:
  /// Constructs a unary operator instruction.
  UnaryInst(Kind kind, Type type, Inst *arg, const AnnotSet &annot);

  /// Returns the sole argument.
  Inst *GetArg() const;

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

/**
 * Instructions with two operands.
 */
class BinaryInst : public OperatorInst {
public:
  /// Constructs a binary operator instruction.
  BinaryInst(Kind kind, Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot);

  /// Returns the LHS operator.
  Inst *GetLHS() const;
  /// Returns the RHS operator.
  Inst *GetRHS() const;

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

/**
 * Overflow-checking instructions.
 */
class OverflowInst : public BinaryInst {
public:
  /// Constructs an overflow-checking instruction.
  OverflowInst(Kind kind, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(kind, Type::I32, lhs, rhs, annot)
  {
  }

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};
