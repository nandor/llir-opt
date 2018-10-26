// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "inst.h"



class CallInst final : public FlowInst {
public:
  CallInst(const Operand &callee, const std::vector<Operand> &ops)
  {
  }

  CallInst(Type type, const Operand &callee, const std::vector<Operand> &ops)
  {
  }
};

class TailCallInst final : public TerminatorInst {
public:
  TailCallInst(const Operand &callee, const std::vector<Operand> &ops)
  {
  }

  TailCallInst(Type type, const Operand &callee, const std::vector<Operand> &ops)
  {
  }
};

class JumpTrueInst final : public TerminatorInst {
public:
  JumpTrueInst(const Operand &cond, const Operand &target)
  {
  }
};

class JumpFalseInst final : public TerminatorInst {
public:
  JumpFalseInst(const Operand &cond, const Operand &target)
  {
  }
};

class JumpIndirectInst final : public TerminatorInst {
public:
  JumpIndirectInst(const Operand &target)
  {
  }
};

class JumpInst final : public TerminatorInst {
public:
  JumpInst(const Operand &target)
  {
  }
};

class ReturnInst final : public TerminatorInst {
public:
  ReturnInst()
  {
  }

  ReturnInst(Type t, const Operand &op)
  {
  }
};

class SwitchInst final : public TerminatorInst {
public:
  SwitchInst(const Operand &op, const std::vector<Operand> &ops)
  {
  }
};

class LoadInst final : public MemoryInst {
public:
  LoadInst(Type type, const Operand &addr)
  {
  }
};

class StoreInst final : public MemoryInst {
public:
  StoreInst(Type type, const Operand &addr, const Operand &val)
  {
  }
};

class PushInst final : public StackInst {
public:
  PushInst(Type type, const Operand &val)
  {
  }
};

class PopInst final : public StackInst {
public:
  PopInst(Type type)
  {
  }
};

class SelectInst final : public OperatorInst {
public:
  SelectInst(Type type, const Operand &cond, const Operand &vt, const Operand &vf)
  {
  }
};

class ExchangeInst final : public MemoryInst {
public:
  ExchangeInst(Type type, const Operand &addr, const Operand &val)
  {
  }
};

class ImmediateInst final : public ConstInst {
public:
  ImmediateInst(Type type, int64_t imm)
  {
  }
};

class ArgInst final : public ConstInst {
public:
  ArgInst(Type type, unsigned index)
  {
  }
};

class AddrInst final : public ConstInst {
public:
  AddrInst(Type type, const Operand &addr)
  {
  }
};

class AbsInst final : public UnaryOperatorInst {
public:
  AbsInst(Type type, const Operand &op)
    : UnaryOperatorInst(type, op)
  {
  }
};

class MovInst final : public UnaryOperatorInst {
public:
  MovInst(Type type, const Operand &op)
    : UnaryOperatorInst(type, op)
  {
  }
};

class NegInst final : public UnaryOperatorInst {
public:
  NegInst(Type type, const Operand &op)
    : UnaryOperatorInst(type, op)
  {
  }
};

class SignExtendInst final : public UnaryOperatorInst {
public:
  SignExtendInst(Type type, const Operand &op)
    : UnaryOperatorInst(type, op)
  {
  }
};

class ZeroExtendInst final : public UnaryOperatorInst {
public:
  ZeroExtendInst(Type type, const Operand &op)
    : UnaryOperatorInst(type, op)
  {
  }
};

class TruncateInst final : public UnaryOperatorInst {
public:
  TruncateInst(Type type, const Operand &op)
    : UnaryOperatorInst(type, op)
  {
  }
};

class AddInst final : public BinaryOperatorInst {
public:
  AddInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class AndInst final : public BinaryOperatorInst {
public:
  AndInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class AsrInst final : public BinaryOperatorInst {
public:
  AsrInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class CmpInst final : public BinaryOperatorInst {
public:
  CmpInst(Type type, Cond cc, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class DivInst final : public BinaryOperatorInst {
public:
  DivInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class LslInst final : public BinaryOperatorInst {
public:
  LslInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class LsrInst final : public BinaryOperatorInst {
public:
  LsrInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class ModInst final : public BinaryOperatorInst {
public:
  ModInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class MulInst final : public BinaryOperatorInst {
public:
  MulInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class MulhInst final : public BinaryOperatorInst {
public:
  MulhInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class OrInst final : public BinaryOperatorInst {
public:
  OrInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class RemInst final : public BinaryOperatorInst {
public:
  RemInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class RotlInst final : public BinaryOperatorInst {
public:
  RotlInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class ShlInst final : public BinaryOperatorInst {
public:
  ShlInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class SraInst final : public BinaryOperatorInst {
public:
  SraInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class SrlInst final : public BinaryOperatorInst {
public:
  SrlInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class SubInst final : public BinaryOperatorInst {
public:
  SubInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};

class XorInst final : public BinaryOperatorInst {
public:
  XorInst(Type type, const Operand &lhs, const Operand &rhs)
    : BinaryOperatorInst(type, lhs, rhs)
  {
  }
};
