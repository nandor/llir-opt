// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <optional>
#include "inst.h"
#include "calling_conv.h"



/**
 * Base class for call instructions.
 */
template<typename T>
class CallSite : public T {
public:
  CallSite(
      Inst::Kind kind,
      Block *parent,
      unsigned numOps,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv
  );

  /// Checks if the function is var arg: more args than fixed ones.
  bool IsVarArg() const { return numArgs_ > numFixed_; }
  /// Returns the number of fixed arguments.
  unsigned GetNumFixedArgs() const { return numFixed_; }
  /// Returns the calling convention.
  CallingConv GetCallingConv() const { return callConv_; }
  virtual std::optional<size_t> GetSize() const { return numFixed_; }

private:
  /// Number of actual arguments.
  unsigned numArgs_;
  /// Number of fixed arguments.
  unsigned numFixed_;
  /// Calling convention of the call.
  CallingConv callConv_;
};


/**
 * CallInst
 */
class CallInst final : public CallSite<ControlInst> {
public:
  /// Creates a void call.
  CallInst(
      Block *block,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv)
    : CallInst(
          block,
          std::nullopt,
          callee,
          args,
          numFixed,
          callConv
      )
  {
  }

  /// Creates a call returning a value.
  CallInst(
      Block *block,
      Type type,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv)
    : CallInst(
          block,
          std::optional<Type>(type),
          callee,
          args,
          numFixed,
          callConv
      )
  {
  }

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns the callee.
  Inst *GetCallee() const { return static_cast<Inst *>(Op<0>().get()); }

private:
  /// Initialises the call instruction.
  CallInst(
      Block *block,
      std::optional<Type> type,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv
  );

private:
  /// Returns the type of the return value.
  std::optional<Type> type_;
};

/**
 * TailCallInst
 */
class TailCallInst final : public CallSite<TerminatorInst> {
public:
  TailCallInst(
      Block *block,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;
};

/**
 * InvokeInst
 */
class InvokeInst final : public CallSite<TerminatorInst> {
public:
  InvokeInst(
      Block *block,
      Inst *callee,
      const std::vector<Value *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv)
    : InvokeInst(
          block,
          std::nullopt,
          callee,
          args,
          jcont,
          jthrow,
          numFixed,
          callConv
      )
  {
  }

  InvokeInst(
      Block *block,
      Type type,
      Inst *callee,
      const std::vector<Value *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv)
    : InvokeInst(
          block,
          std::optional<Type>(type),
          callee,
          args,
          jcont,
          jthrow,
          numFixed,
          callConv
      )
  {
  }

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

private:
  /// Initialises the invoke instruction.
  InvokeInst(
      Block *block,
      std::optional<Type> type,
      Inst *callee,
      const std::vector<Value *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv
  );

private:
  /// Returns the type of the return value.
  std::optional<Type> type_;
};


/**
 * JumpCondInst
 */
class JumpCondInst final : public TerminatorInst {
public:
  JumpCondInst(
      Block *block,
      Value *cond,
      Block *bt,
      Block *bf
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the condition.
  Inst *GetCond() const;
  /// Returns the true target.
  Block *GetTrueTarget() const;
  /// Returns the false target.
  Block *GetFalseTarget() const;
};

/**
 * JumpIndirectInst
 */
class JumpIndirectInst final : public TerminatorInst {
public:
  JumpIndirectInst(Block *block, Value *target);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;
};

/**
 * JumpInst
 */
class JumpInst final : public TerminatorInst {
public:
  JumpInst(Block *block, Value *target);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;
};

/**
 * ReturnInst
 */
class ReturnInst final : public TerminatorInst {
public:
  ReturnInst(Block *block);
  ReturnInst(Block *block, Type t, Inst *op);

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the return value.
  Inst *GetValue() const;
};

/**
 * SwitchInst
 */
class SwitchInst final : public TerminatorInst {
public:
  SwitchInst(
      Block *block,
      Inst *index,
      const std::vector<Value *> &branches
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;
};

/**
 * Trap instruction which terminates a block.
 */
class TrapInst final : public TerminatorInst {
public:
  TrapInst(Block *block) : TerminatorInst(Kind::TRAP, block, 0) { }

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
  LoadInst(Block *block, size_t size, Type type, Value *addr);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns the size of the instruction.
  std::optional<size_t> GetSize() const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }
  /// Returns the size of the read.
  size_t GetLoadSize() const { return size_; }
  /// Returns the address instruction.
  const Inst *GetAddr() const;

private:
  /// Size of the load.
  size_t size_;
  /// Type of the instruction.
  Type type_;
};

/**
 * StoreInst
 */
class StoreInst final : public MemoryInst {
public:
  StoreInst(Block *block, size_t size, Inst *addr, Inst *val);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns the size of the instruction.
  std::optional<size_t> GetSize() const override;

  /// Returns the size of the store.
  size_t GetStoreSize() const { return size_; }
  /// Returns the address to store the value at.
  const Inst *GetAddr() const;
  /// Returns the value to store.
  const Inst *GetVal() const;

private:
  /// Size of the store.
  size_t size_;
};

/**
 * PushInst
 */
class PushInst final : public StackInst {
public:
  PushInst(Block *block, Type type, Inst *val);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
};

/**
 * PopInst
 */
class PopInst final : public StackInst {
public:
  PopInst(Block *block, Type type)
    : StackInst(Kind::POP, block, 0)
    , type_(type)
  {
  }

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

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
      Inst *cond,
      Inst *vt,
      Inst *vf
  );
};

/**
 * ExchangeInst
 */
class ExchangeInst final : public MemoryInst {
public:
  ExchangeInst(
      Block *block,
      Type type,
      Inst *addr,
      Inst *val
  );

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

private:
  /// Type of the instruction.
  Type type_;
};

/**
 * SetInst
 */
class SetInst final : public Inst {
public:
  SetInst(Block *block, Value *reg, Inst *val);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
};

/**
 * MovInst
 */
class MovInst final : public OperatorInst {
public:
  MovInst(Block *block, Type type, Value *op)
    : OperatorInst(Kind::MOV, block, type, 1)
  {
    Op<0>() = op;
  }

  Value *GetOp() const { return static_cast<Value *>(Op<0>().get()); }
};

/**
 * ImmInst
 */
class ImmInst final : public ConstInst {
public:
  ImmInst(Block *block, Type type, Constant *imm);

  /// Returns the immediate value.
  int64_t GetInt() const;
  /// Returns the immediate value.
  double GetFloat() const;
};

/**
 * ArgInst
 */
class ArgInst final : public ConstInst {
public:
  ArgInst(Block *block, Type type, ConstantInt *index);

  /// Returns the argument index.
  unsigned GetIdx() const;
};

/**
 * AddrInst
 */
class AddrInst final : public ConstInst {
public:
  AddrInst(Block *block, Type type, Value *addr);

  /// Returns the argument.
  Value *GetAddr() const;
};

/**
 * AbsInst
 */
class AbsInst final : public UnaryInst {
public:
  AbsInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::ABS, block, type, op)
  {
  }
};

/**
 * NegInst
 */
class NegInst final : public UnaryInst {
public:
  NegInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::NEG, block, type, op)
  {
  }
};

/**
 * SqrtInst
 */
class SqrtInst final : public UnaryInst {
public:
  SqrtInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::SQRT, block, type, op)
  {
  }
};

/**
 * SinInst
 */
class SinInst final : public UnaryInst {
public:
  SinInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::SIN, block, type, op)
  {
  }
};

/**
 * CosInst
 */
class CosInst final : public UnaryInst {
public:
  CosInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::COS, block, type, op)
  {
  }
};

/**
 * SignExtendInst
 */
class SignExtendInst final : public UnaryInst {
public:
  SignExtendInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::SEXT, block, type, op)
  {
  }
};

/**
 * ZeroExtendInst
 */
class ZeroExtendInst final : public UnaryInst {
public:
  ZeroExtendInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::ZEXT, block, type, op)
  {
  }
};

/**
 * TruncateInst
 */
class TruncateInst final : public UnaryInst {
public:
  TruncateInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::TRUNC, block, type, op)
  {
  }
};

/**
 * AddInst
 */
class AddInst final : public BinaryInst {
public:
  AddInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::ADD, block, type, lhs, rhs)
  {
  }
};

/**
 * AndInst
 */
class AndInst final : public BinaryInst {
public:
  AndInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::AND, block, type, lhs, rhs)
  {
  }
};

/**
 * CmpInst
 */
class CmpInst final : public BinaryInst {
public:
  CmpInst(Block *block, Type type, Cond cc, Inst *lhs, Inst *rhs)
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
  DivInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::DIV, block, type, lhs, rhs)
  {
  }
};

/**
 * MulInst
 */
class MulInst final : public BinaryInst {
public:
  MulInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::MUL, block, type, lhs, rhs)
  {
  }
};

/**
 * OrInst
 */
class OrInst final : public BinaryInst {
public:
  OrInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::OR, block, type, lhs, rhs)
  {
  }
};

/**
 * RemInst
 */
class RemInst final : public BinaryInst {
public:
  RemInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::REM, block, type, lhs, rhs)
  {
  }
};

/**
 * RotlInst
 */
class RotlInst final : public BinaryInst {
public:
  RotlInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::ROTL, block, type, lhs, rhs)
  {
  }
};

/**

 * SllInst
 */

class SllInst final : public BinaryInst {
public:
  SllInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SLL, block, type, lhs, rhs)
  {
  }
};

/**
 * SraInst
 */
class SraInst final : public BinaryInst {
public:
  SraInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SRA, block, type, lhs, rhs)
  {
  }
};

/**
 * SrlInst
 */
class SrlInst final : public BinaryInst {
public:
  SrlInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SRL, block, type, lhs, rhs)
  {
  }
};

/**
 * SubInst
 */
class SubInst final : public BinaryInst {
public:
  SubInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SUB, block, type, lhs, rhs)
  {
  }
};

/**
 * XorInst
 */
class XorInst final : public BinaryInst {
public:
  XorInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::XOR, block, type, lhs, rhs)
  {
  }
};

/**
 * PowInst
 */
class PowInst final : public BinaryInst {
public:
  PowInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::POW, block, type, lhs, rhs)
  {
  }
};

/**
 * CopySignInst
 */
class CopySignInst final : public BinaryInst {
public:
  CopySignInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::POW, block, type, lhs, rhs)
  {
  }
};

/**
 * PHI instruction.
 */
class PhiInst final : public Inst {
public:
  PhiInst(Block *block, Type type);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Adds an incoming value.
  void Add(Block *block, Value *value);
  /// Returns the number of predecessors.
  unsigned GetNumIncoming() const;
  /// Returns the nth block.
  Block *GetBlock(unsigned i) const;
  /// Returns the nth value.
  Value *GetValue(unsigned i) const;

  /// Returns the immediate type.
  Type GetType() const { return type_; }
  /// Checks if the PHI has a value for a block.
  bool HasValue(const Block *block) const;
  /// Returns an operand for a block.
  Value *GetValue(const Block *block) const;

private:
  /// Type of the PHI node.
  Type type_;
};
