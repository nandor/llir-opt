// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "inst.h"



class CallInst final : public ControlInst {
public:
  CallInst(
      const Operand &callee,
      const std::vector<Operand> &ops)
    : ControlInst(Kind::CALL)
  {
  }

  CallInst(
      Type type,
      const Operand &callee,
      const std::vector<Operand> &ops)
    : ControlInst(Kind::CALL)
  {
  }
};

class TailCallInst final : public TerminatorInst {
public:
  TailCallInst(
      const Operand &callee,
      const std::vector<Operand> &ops)
    : TerminatorInst(Kind::TCALL)
  {
  }

  TailCallInst(
      Type type,
      const Operand &callee,
      const std::vector<Operand> &ops)
    : TerminatorInst(Kind::TCALL)
  {
  }
};

class JumpTrueInst final : public TerminatorInst {
public:
  JumpTrueInst(const Operand &cond, const Operand &target)
    : TerminatorInst(Kind::JT)
  {
  }
};

class JumpFalseInst final : public TerminatorInst {
public:
  JumpFalseInst(const Operand &cond, const Operand &target)
    : TerminatorInst(Kind::JF)
  {
  }
};

class JumpIndirectInst final : public TerminatorInst {
public:
  JumpIndirectInst(const Operand &target)
    : TerminatorInst(Kind::JI)
  {
  }
};

class JumpInst final : public TerminatorInst {
public:
  JumpInst(const Operand &target)
    : TerminatorInst(Kind::JMP)
  {
  }
};

class ReturnInst final : public TerminatorInst {
public:
  ReturnInst()
    : TerminatorInst(Kind::RET)
  {
  }

  ReturnInst(Type t, const Operand &op)
    : TerminatorInst(Kind::RET)
  {
  }
};

class SwitchInst final : public TerminatorInst {
public:
  SwitchInst(const Operand &op, const std::vector<Operand> &ops)
    : TerminatorInst(Kind::SWITCH)
  {
  }
};

class LoadInst final : public MemoryInst {
public:
  LoadInst(Type type, const Operand &addr)
    : MemoryInst(Kind::LD)
  {
  }
};

class StoreInst final : public MemoryInst {
public:
  StoreInst(Type type, const Operand &addr, const Operand &val)
    : MemoryInst(Kind::ST)
  {
  }
};

class PushInst final : public StackInst {
public:
  PushInst(Type type, const Operand &val)
    : StackInst(Kind::PUSH)
  {
  }
};

class PopInst final : public StackInst {
public:
  PopInst(Type type)
    : StackInst(Kind::POP)
  {
  }
};

class SelectInst final : public OperatorInst {
public:
  SelectInst(
      Type type,
      const Operand &cond,
      const Operand &vt,
      const Operand &vf)
    : OperatorInst(Kind::SELECT)
  {
  }
};

class ExchangeInst final : public MemoryInst {
public:
  ExchangeInst(Type type, const Operand &addr, const Operand &val)
    : MemoryInst(Kind::XCHG)
  {
  }
};

class ImmediateInst final : public ConstInst {
public:
  ImmediateInst(Type type, int64_t imm)
    : ConstInst(Kind::IMM)
  {
  }
};

class ArgInst final : public ConstInst {
public:
  ArgInst(Type type, unsigned index)
    : ConstInst(Kind::ARG)
  {
  }
};

class AddrInst final : public ConstInst {
public:
  AddrInst(Type type, const Operand &addr)
    : ConstInst(Kind::ADDR)
  {
  }
};

class AbsInst final : public UnaryOperatorInst {
public:
  AbsInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::ABS, type, op)
  {
  }
};

class MovInst final : public UnaryOperatorInst {
public:
  MovInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::MOV, type, op)
  {
  }
};

class NegInst final : public UnaryOperatorInst {
public:
  NegInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::NEG, type, op)
  {
  }
};

class SignExtendInst final : public UnaryOperatorInst {
public:
  SignExtendInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::SEXT, type, op)
  {
  }
};

class ZeroExtendInst final : public UnaryOperatorInst {
public:
  ZeroExtendInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::ZEXT, type, op)
  {
  }
};

class TruncateInst final : public UnaryOperatorInst {
public:
  TruncateInst(Type type, const Operand &op)
    : UnaryOperatorInst(Kind::TRUNC, type, op)
  {
  }
};

class AddInst final : public BinaryOperatorInst {
public:
  AddInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::ADD, type, lhs, rhs)
  {
  }
};

class AndInst final : public BinaryOperatorInst {
public:
  AndInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::AND, type, lhs, rhs)
  {
  }
};

class AsrInst final : public BinaryOperatorInst {
public:
  AsrInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::ASR, type, lhs, rhs)
  {
  }
};

class CmpInst final : public BinaryOperatorInst {
public:
  CmpInst(Type type, Cond cc, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::CMP, type, lhs, rhs)
  {
  }
};

class DivInst final : public BinaryOperatorInst {
public:
  DivInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::DIV, type, lhs, rhs)
  {
  }
};

class LslInst final : public BinaryOperatorInst {
public:
  LslInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::LSL, type, lhs, rhs)
  {
  }
};

class LsrInst final : public BinaryOperatorInst {
public:
  LsrInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::LSR, type, lhs, rhs)
  {
  }
};

class ModInst final : public BinaryOperatorInst {
public:
  ModInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::MOD, type, lhs, rhs)
  {
  }
};

class MulInst final : public BinaryOperatorInst {
public:
  MulInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::MUL, type, lhs, rhs)
  {
  }
};

class MulhInst final : public BinaryOperatorInst {
public:
  MulhInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::MULH, type, lhs, rhs)
  {
  }
};

class OrInst final : public BinaryOperatorInst {
public:
  OrInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::OR, type, lhs, rhs)
  {
  }
};

class RemInst final : public BinaryOperatorInst {
public:
  RemInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::REM, type, lhs, rhs)
  {
  }
};

class RotlInst final : public BinaryOperatorInst {
public:
  RotlInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::ROTL, type, lhs, rhs)
  {
  }
};

class ShlInst final : public BinaryOperatorInst {
public:
  ShlInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::SHL, type, lhs, rhs)
  {
  }
};

class SraInst final : public BinaryOperatorInst {
public:
  SraInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::SRA, type, lhs, rhs)
  {
  }
};

class SrlInst final : public BinaryOperatorInst {
public:
  SrlInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::SRL, type, lhs, rhs)
  {
  }
};

class SubInst final : public BinaryOperatorInst {
public:
  SubInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::SUB, type, lhs, rhs)
  {
  }
};

class XorInst final : public BinaryOperatorInst {
public:
  XorInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(Kind::XOR, type, lhs, rhs)
  {
  }
};
