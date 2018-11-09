// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <optional>
#include "inst.h"



/**
 * CallInst
 */
class CallInst final : public ControlInst {
public:
  CallInst(
      Block *block,
      const Operand &callee,
      const std::vector<Operand> &args)
    : ControlInst(Kind::CALL, block)
    , callee_(callee)
    , args_(args)
  {
  }

  CallInst(
      Block *block,
      Type type,
      const Operand &callee,
      const std::vector<Operand> &args)
    : ControlInst(Kind::CALL, block)
    , type_(type)
    , callee_(callee)
    , args_(args)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Returns the type of the return value.
  std::optional<Type> type_;
  /// Called function: direct symbol or indirect value.
  Operand callee_;
  /// List of arguments.
  std::vector<Operand> args_;
};

/**
 * TailCallInst
 */
class TailCallInst final : public TerminatorInst {
public:
  TailCallInst(
      Block *block,
      const Operand &callee,
      const std::vector<Operand> &args)
    : TerminatorInst(Kind::TCALL, block)
    , callee_(callee)
    , args_(args)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

private:
  /// Called function: direct symbol or indirect value.
  Operand callee_;
  /// List of arguments.
  std::vector<Operand> args_;
};

/**
 * InvokeInst
 */
class InvokeInst final : public TerminatorInst {
public:
  InvokeInst(
      Block *block,
      const Operand &callee,
      const std::vector<Operand> &args,
      const Operand &jcont,
      const Operand &jthrow)
    : TerminatorInst(Kind::INVOKE, block)
    , callee_(callee)
    , args_(args)
    , jcont_(jcont)
    , jthrow_(jthrow)
  {
  }

  InvokeInst(
      Block *block,
      Type type,
      const Operand &callee,
      const std::vector<Operand> &args,
      const Operand &jcont,
      const Operand &jthrow)
    : TerminatorInst(Kind::INVOKE, block)
    , type_(type)
    , callee_(callee)
    , args_(args)
    , jcont_(jcont)
    , jthrow_(jthrow)
  {
  }


  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

private:
  /// Returns the type of the return value.
  std::optional<Type> type_;
  /// Called function: direct symbol or indirect value.
  Operand callee_;
  /// List of arguments.
  std::vector<Operand> args_;
  /// Continuation.
  Operand jcont_;
  /// Exception branch.
  Operand jthrow_;
};


/**
 * JumpCondInst
 */
class JumpCondInst final : public TerminatorInst {
public:
  JumpCondInst(
      Block *block,
      const Operand &cond,
      const Operand &bt,
      const Operand &bf)
    : TerminatorInst(Kind::JCC, block)
    , cond_(cond)
    , bt_(bt)
    , bf_(bf)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the condition.
  Inst *GetCond() const { return cond_.GetInst(); }
  /// Returns the true target.
  Block *GetTrueTarget() const { return bt_.GetBlock(); }
  /// Returns the false target.
  Block *GetFalseTarget() const { return bf_.GetBlock(); }

private:
  /// Jump condition.
  Operand cond_;
  /// True target.
  Operand bt_;
  /// False target.
  Operand bf_;
};

/**
 * JumpIndirectInst
 */
class JumpIndirectInst final : public TerminatorInst {
public:
  JumpIndirectInst(Block *block, const Operand &target)
    : TerminatorInst(Kind::JI, block)
    , target_(target)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

private:
  /// Jump target.
  Operand target_;
};

/**
 * JumpInst
 */
class JumpInst final : public TerminatorInst {
public:
  JumpInst(Block *block, const Operand &target)
    : TerminatorInst(Kind::JMP, block)
    , target_(target)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

private:
  /// Jump target.
  Operand target_;
};

/**
 * ReturnInst
 */
class ReturnInst final : public TerminatorInst {
public:
  ReturnInst(Block *block)
    : TerminatorInst(Kind::RET, block)
  {
  }

  ReturnInst(Block *block, Type t, const Operand &op)
    : TerminatorInst(Kind::RET, block)
    , op_(op)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the return value.
  Inst *GetValue() const { return op_ ? op_->GetInst() : nullptr; }

private:
  /// Optional return value.
  std::optional<Operand> op_;
};

/**
 * SwitchInst
 */
class SwitchInst final : public TerminatorInst {
public:
  SwitchInst(
      Block *block,
      const Operand &index,
      const std::vector<Operand> &branches)
    : TerminatorInst(Kind::SWITCH, block)
    , index_(index)
    , branches_(branches)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

private:
  /// Index.
  Operand index_;
  /// Jump table.
  std::vector<Operand> branches_;
};

/**
 * Trap instruction which terminates a block.
 */
class TrapInst final : public TerminatorInst {
public:
  TrapInst(Block *block) : TerminatorInst(Kind::TRAP, block) { }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;
};

/**
 * LoadInst
 */
class LoadInst final : public MemoryInst {
public:
  LoadInst(Block *block, size_t size, Type type, const Operand &addr)
    : MemoryInst(Kind::LD, block)
    , size_(size)
    , type_(type)
    , addr_(addr)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;
  /// Returns the size of the instruction.
  std::optional<size_t> GetSize() const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }
  /// Returns the size of the read.
  size_t GetLoadSize() const { return size_; }
  /// Returns the address instruction.
  const Inst *GetAddr() const { return addr_.GetInst(); }

private:
  /// Size of the load.
  size_t size_;
  /// Type of the instruction.
  Type type_;
  /// Address (instruction).
  Operand addr_;
};

/**
 * StoreInst
 */
class StoreInst final : public MemoryInst {
public:
  StoreInst(Block *block, size_t size, const Operand &addr, const Operand &val)
    : MemoryInst(Kind::ST, block)
    , size_(size)
    , addr_(addr)
    , val_(val)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;
  /// Returns the size of the instruction.
  std::optional<size_t> GetSize() const override;

  /// Returns the size of the store.
  size_t GetStoreSize() const { return size_; }
  /// Returns the address to store the value at.
  const Inst *GetAddr() const { return addr_.GetInst(); }
  /// Returns the value to store.
  const Inst *GetVal() const { return val_.GetInst(); }

private:
  /// Size of the store.
  size_t size_;
  /// Address to store a value at.
  Operand addr_;
  /// Value to store.
  Operand val_;
};

/**
 * PushInst
 */
class PushInst final : public StackInst {
public:
  PushInst(Block *block, Type type, const Operand &val)
    : StackInst(Kind::PUSH, block)
    , val_(val)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Value to be pushed.
  Operand val_;
};

/**
 * PopInst
 */
class PopInst final : public StackInst {
public:
  PopInst(Block *block, Type type)
    : StackInst(Kind::POP, block)
    , type_(type)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Type of the instruction.
  Type type_;
};

/**
 * SelectInst
 */
class SelectInst final : public OperatorInst {
public:
  SelectInst(
      Block *block,
      Type type,
      const Operand &cond,
      const Operand &vt,
      const Operand &vf)
    : OperatorInst(Kind::SELECT, block)
    , type_(type)
    , cond_(cond)
    , vt_(vt)
    , vf_(vf)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Type of the instruction.
  Type type_;
  /// Condition value.
  Operand cond_;
  /// Value if true.
  Operand vt_;
  /// Value if false.
  Operand vf_;
};

/**
 * ExchangeInst
 */
class ExchangeInst final : public MemoryInst {
public:
  ExchangeInst(Block *block, Type type, const Operand &addr, const Operand &val)
    : MemoryInst(Kind::XCHG, block)
    , type_(type)
    , addr_(addr)
    , val_(val)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Type of the instruction.
  Type type_;
  /// Target address.
  Operand addr_;
  /// New value.
  Operand val_;
};

/**
 * SetInst
 */
class SetInst final : public Inst {
public:
  SetInst(Block *block, const Operand &reg, const Operand &val)
    : Inst(Kind::SET, block)
    , reg_(reg)
    , val_(val)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Register to set.
  Operand reg_;
  /// Value to set register to.
  Operand val_;
};

/**
 * ImmInst
 */
class ImmInst final : public ConstInst {
public:
  ImmInst(Block *block, Type type, int64_t imm)
    : ConstInst(Kind::IMM, block)
    , type_(type)
    , imm_(imm)
  {
  }

  ImmInst(Block *block, Type type, double imm)
    : ConstInst(Kind::IMM, block)
    , type_(type)
    , imm_(imm)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an immutable operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the immediate type.
  Type GetType() const { return type_; }
  /// Returns the immediate value.
  int64_t GetInt() const;
  /// Returns the immediate value.
  double GetFloat() const;

private:
  /// Type of the instruction.
  Type type_;
  /// Encoded immediate value.
  Operand imm_;
};

/**
 * ArgInst
 */
class ArgInst final : public ConstInst {
public:
  ArgInst(Block *block, Type type, unsigned index)
    : ConstInst(Kind::ARG, block)
    , type_(type)
    , index_(static_cast<int64_t>(index))
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an immutable operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Returns the argument type.
  Type GetType() const { return type_; }
  /// Returns the argument index.
  unsigned GetIdx() const { return index_.GetInt(); }

private:
  /// Type of the instruction.
  Type type_;
  /// Encoded index.
  Operand index_;
};

/**
 * AddrInst
 */
class AddrInst final : public ConstInst {
public:
  AddrInst(Block *block, Type type, const Operand &addr)
    : ConstInst(Kind::ADDR, block)
    , type_(type)
    , addr_(addr)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Type of the instruction.
  Type type_;
  /// Referenced address (symbol or block).
  Operand addr_;
};

/**
 * AbsInst
 */
class AbsInst final : public UnaryInst {
public:
  AbsInst(Block *block, Type type, const Operand &op)
    : UnaryInst(Kind::ABS, block, type, op)
  {
  }
};

/**
 * MovInst
 */
class MovInst final : public UnaryInst {
public:
  MovInst(Block *block, Type type, const Operand &op)
    : UnaryInst(Kind::MOV, block, type, op)
  {
  }
};

/**
 * NegInst
 */
class NegInst final : public UnaryInst {
public:
  NegInst(Block *block, Type type, const Operand &op)
    : UnaryInst(Kind::NEG, block, type, op)
  {
  }
};

/**
 * SignExtendInst
 */
class SignExtendInst final : public UnaryInst {
public:
  SignExtendInst(Block *block, Type type, const Operand &op)
    : UnaryInst(Kind::SEXT, block, type, op)
  {
  }
};

/**
 * ZeroExtendInst
 */
class ZeroExtendInst final : public UnaryInst {
public:
  ZeroExtendInst(Block *block, Type type, const Operand &op)
    : UnaryInst(Kind::ZEXT, block, type, op)
  {
  }
};

/**
 * TruncateInst
 */
class TruncateInst final : public UnaryInst {
public:
  TruncateInst(Block *block, Type type, const Operand &op)
    : UnaryInst(Kind::TRUNC, block, type, op)
  {
  }
};

/**
 * AddInst
 */
class AddInst final : public BinaryInst {
public:
  AddInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::ADD, block, type, lhs, rhs)
  {
  }
};

/**
 * AndInst
 */
class AndInst final : public BinaryInst {
public:
  AndInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::AND, block, type, lhs, rhs)
  {
  }
};

/**
 * CmpInst
 */
class CmpInst final : public BinaryInst {
public:
  CmpInst(Block *block, Type type, Cond cc, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::CMP, block, type, lhs, rhs)
    , cc_(cc)
  {
  }

  /// Returns the condition code.
  Cond GetCC() const { return cc_; }

private:
  /// Condition code.
  Cond cc_;
};

/**
 * DivInst
 */
class DivInst final : public BinaryInst {
public:
  DivInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::DIV, block, type, lhs, rhs)
  {
  }
};

/**
 * MulInst
 */
class MulInst final : public BinaryInst {
public:
  MulInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::MUL, block, type, lhs, rhs)
  {
  }
};

/**
 * MulhInst
 */
class MulhInst final : public BinaryInst {
public:
  MulhInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::MULH, block, type, lhs, rhs)
  {
  }
};

/**
 * OrInst
 */
class OrInst final : public BinaryInst {
public:
  OrInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::OR, block, type, lhs, rhs)
  {
  }
};

/**
 * RemInst
 */
class RemInst final : public BinaryInst {
public:
  RemInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::REM, block, type, lhs, rhs)
  {
  }
};

/**
 * RotlInst
 */
class RotlInst final : public BinaryInst {
public:
  RotlInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::ROTL, block, type, lhs, rhs)
  {
  }
};

/**

 * SllInst
 */

class SllInst final : public BinaryInst {
public:
  SllInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::SLL, block, type, lhs, rhs)
  {
  }
};

/**
 * SraInst
 */
class SraInst final : public BinaryInst {
public:
  SraInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::SRA, block, type, lhs, rhs)
  {
  }
};

/**
 * SrlInst
 */
class SrlInst final : public BinaryInst {
public:
  SrlInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::SRL, block, type, lhs, rhs)
  {
  }
};

/**
 * SubInst
 */
class SubInst final : public BinaryInst {
public:
  SubInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::SUB, block, type, lhs, rhs)
  {
  }
};

/**
 * XorInst
 */
class XorInst final : public BinaryInst {
public:
  XorInst(Block *block, Type type, const Operand &lhs, const Operand &rhs)
    : BinaryInst(Kind::XOR, block, type, lhs, rhs)
  {
  }
};

/**
 * PHI instruction.
 */
class PhiInst final : public Inst {
public:
  PhiInst(Block *block, Type type)
    : Inst(Kind::PHI, block)
    , type_(type)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Adds an incoming value.
  void Add(Block *block, const Operand &value);
  /// Returns the number of predecessors.
  unsigned GetNumIncoming() const;
  /// Returns the nth block.
  Block *GetBlock(unsigned i) const;
  /// Returns the nth value.
  const Operand &GetValue(unsigned i) const;

private:
  /// Type of the PHI node.
  Type type_;
  /// Incoming values.
  std::vector<std::pair<Operand, Operand>> ops_;
};
