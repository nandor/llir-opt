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
      const Operand &callee,
      const std::vector<Operand> &args)
    : ControlInst(Kind::CALL)
    , callee_(callee)
    , args_(args)
  {
  }

  CallInst(
      Type type,
      const Operand &callee,
      const std::vector<Operand> &args)
    : ControlInst(Kind::CALL)
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
      const Operand &callee,
      const std::vector<Operand> &args)
    : TerminatorInst(Kind::TCALL)
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

private:
  /// Called function: direct symbol or indirect value.
  Operand callee_;
  /// List of arguments.
  std::vector<Operand> args_;
};

/**
 * JumpTrueInst
 */
class JumpTrueInst final : public TerminatorInst {
public:
  JumpTrueInst(const Operand &cond, const Operand &target)
    : TerminatorInst(Kind::JT)
    , cond_(cond)
    , target_(target)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Checks if the instruction fall through another block.
  bool IsFallthrough() const override { return true; }

private:
  /// Jump condition.
  Operand cond_;
  /// Jump target.
  Operand target_;
};

/**
 * JumpFalseInst
 */
class JumpFalseInst final : public TerminatorInst {
public:
  JumpFalseInst(const Operand &cond, const Operand &target)
    : TerminatorInst(Kind::JF)
    , cond_(cond)
    , target_(target)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

  /// Checks if the instruction fall through another block.
  bool IsFallthrough() const override { return true; }

private:
  /// Jump condition.
  Operand cond_;
  /// Jump target.
  Operand target_;
};

/**
 * JumpIndirectInst
 */
class JumpIndirectInst final : public TerminatorInst {
public:
  JumpIndirectInst(const Operand &target)
    : TerminatorInst(Kind::JI)
    , target_(target)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Jump target.
  Operand target_;
};

/**
 * JumpInst
 */
class JumpInst final : public TerminatorInst {
public:
  JumpInst(const Operand &target)
    : TerminatorInst(Kind::JMP)
    , target_(target)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Jump target.
  Operand target_;
};

/**
 * ReturnInst
 */
class ReturnInst final : public TerminatorInst {
public:
  ReturnInst()
    : TerminatorInst(Kind::RET)
  {
  }

  ReturnInst(Type t, const Operand &op)
    : TerminatorInst(Kind::RET)
    , op_(op)
  {
  }

  /// Returns the number of operands.
  unsigned GetNumOps() const override;
  /// Returns an operand.
  const Operand &GetOp(unsigned i) const override;
  /// Sets an operand.
  void SetOp(unsigned i, const Operand &op) override;

private:
  /// Optional return value.
  std::optional<Operand> op_;
};

/**
 * SwitchInst
 */
class SwitchInst final : public TerminatorInst {
public:
  SwitchInst(const Operand &index, const std::vector<Operand> &branches)
    : TerminatorInst(Kind::SWITCH)
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

private:
  /// Index.
  Operand index_;
  /// Jump table.
  std::vector<Operand> branches_;
};

/**
 * LoadInst
 */
class LoadInst final : public MemoryInst {
public:
  LoadInst(size_t size, Type type, const Operand &addr)
    : MemoryInst(Kind::LD)
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
  StoreInst(size_t size, const Operand &addr, const Operand &val)
    : MemoryInst(Kind::ST)
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
  PushInst(Type type, const Operand &val)
    : StackInst(Kind::PUSH)
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
  PopInst(Type type)
    : StackInst(Kind::POP)
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
      Type type,
      const Operand &cond,
      const Operand &vt,
      const Operand &vf)
    : OperatorInst(Kind::SELECT)
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
  ExchangeInst(Type type, const Operand &addr, const Operand &val)
    : MemoryInst(Kind::XCHG)
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
  SetInst(const Operand &reg, const Operand &val)
    : Inst(Kind::SET)
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
 * ImmediateInst
 */
class ImmediateInst final : public ConstInst {
public:
  ImmediateInst(Type type, int64_t imm)
    : ConstInst(Kind::IMM)
    , type_(type)
    , imm_(imm)
  {
  }

  ImmediateInst(Type type, float imm)
    : ConstInst(Kind::IMM)
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
  ArgInst(Type type, unsigned index)
    : ConstInst(Kind::ARG)
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
  AddrInst(Type type, const Operand &addr)
    : ConstInst(Kind::ADDR)
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
class AbsInst final : public UnaryOperatorInst {
public:
  AbsInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::ABS, type, op)
  {
  }
};

/**
 * MovInst
 */
class MovInst final : public UnaryOperatorInst {
public:
  MovInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::MOV, type, op)
  {
  }
};

/**
 * NegInst
 */
class NegInst final : public UnaryOperatorInst {
public:
  NegInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::NEG, type, op)
  {
  }
};

/**
 * SignExtendInst
 */
class SignExtendInst final : public UnaryOperatorInst {
public:
  SignExtendInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::SEXT, type, op)
  {
  }
};

/**
 * ZeroExtendInst
 */
class ZeroExtendInst final : public UnaryOperatorInst {
public:
  ZeroExtendInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::ZEXT, type, op)
  {
  }
};

/**
 * TruncateInst
 */
class TruncateInst final : public UnaryOperatorInst {
public:
  TruncateInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::TRUNC, type, op)
  {
  }
};

/**
 * AddInst
 */
class AddInst final : public BinaryOperatorInst {
public:
  AddInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::ADD, type, lhs, rhs)
  {
  }
};

/**
 * AndInst
 */
class AndInst final : public BinaryOperatorInst {
public:
  AndInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::AND, type, lhs, rhs)
  {
  }
};

/**
 * AsrInst
 */
class AsrInst final : public BinaryOperatorInst {
public:
  AsrInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::ASR, type, lhs, rhs)
  {
  }
};

/**
 * CmpInst
 */
class CmpInst final : public BinaryOperatorInst {
public:
  CmpInst(Type type, Cond cc, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::CMP, type, lhs, rhs)
  {
  }
};

/**
 * DivInst
 */
class DivInst final : public BinaryOperatorInst {
public:
  DivInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::DIV, type, lhs, rhs)
  {
  }
};

/**
 * LslInst
 */
class LslInst final : public BinaryOperatorInst {
public:
  LslInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::LSL, type, lhs, rhs)
  {
  }
};

/**
 * LsrInst
 */
class LsrInst final : public BinaryOperatorInst {
public:
  LsrInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::LSR, type, lhs, rhs)
  {
  }
};

/**
 * ModInst
 */
class ModInst final : public BinaryOperatorInst {
public:
  ModInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::MOD, type, lhs, rhs)
  {
  }
};

/**
 * MulInst
 */
class MulInst final : public BinaryOperatorInst {
public:
  MulInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::MUL, type, lhs, rhs)
  {
  }
};

/**
 * MulhInst
 */
class MulhInst final : public BinaryOperatorInst {
public:
  MulhInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::MULH, type, lhs, rhs)
  {
  }
};

/**
 * OrInst
 */
class OrInst final : public BinaryOperatorInst {
public:
  OrInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::OR, type, lhs, rhs)
  {
  }
};

/**
 * RemInst
 */
class RemInst final : public BinaryOperatorInst {
public:
  RemInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::REM, type, lhs, rhs)
  {
  }
};

/**
 * RotlInst
 */
class RotlInst final : public BinaryOperatorInst {
public:
  RotlInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::ROTL, type, lhs, rhs)
  {
  }
};

/**
 * ShlInst
 */
class ShlInst final : public BinaryOperatorInst {
public:
  ShlInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::SHL, type, lhs, rhs)
  {
  }
};

/**
 * SraInst
 */
class SraInst final : public BinaryOperatorInst {
public:
  SraInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::SRA, type, lhs, rhs)
  {
  }
};

/**
 * SrlInst
 */
class SrlInst final : public BinaryOperatorInst {
public:
  SrlInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::SRL, type, lhs, rhs)
  {
  }
};

/**
 * SubInst
 */
class SubInst final : public BinaryOperatorInst {
public:
  SubInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::SUB, type, lhs, rhs)
  {
  }
};

/**
 * XorInst
 */
class XorInst final : public BinaryOperatorInst {
public:
  XorInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::XOR, type, lhs, rhs)
  {
  }
};
