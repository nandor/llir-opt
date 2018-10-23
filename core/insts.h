// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "inst.h"



class CallInst final : public FlowInst {
public:
  ~CallInst();
};

class JumpTrueInst final : public TerminatorInst {
};

class JumpFalseInst final : public TerminatorInst {
};

class JumpIndirectInst final : public TerminatorInst {
};

class ReturnInst final : public TerminatorInst {
};

class TailCallInst final : public TerminatorInst {
};

class SwitchInst final : public TerminatorInst {
};

class LoadInst final : public MemoryInst {
};

class StoreInst final : public MemoryInst {
};

class PushInst final : public StackInst {
};

class PopInst final : public StackInst {
};

class SelectInst final : public OperatorInst {
};

class AtomicExchangeInst final : public AtomicInst {
};

class ImmediateInst final : public ConstInst {
};

class AddrInst final : public ConstInst {
};

class ArgInst final : public ConstInst {
};

class AbsInst final : public UnaryOperatorInst {
};

class MovInst final : public UnaryOperatorInst {
};

class NegInst final : public UnaryOperatorInst {
};

class SignExtendInst final : public UnaryOperatorInst {
};

class ZeroExtendInst final : public UnaryOperatorInst {
};

class TruncateInst final : public UnaryOperatorInst {
};

class AddInst final : public BinaryOperatorInst {
};

class AndInst final : public BinaryOperatorInst {
};

class AsrInst final : public BinaryOperatorInst {
};

class CmpInst final : public BinaryOperatorInst {
};

class DivInst final : public BinaryOperatorInst {
};

class LslInst final : public BinaryOperatorInst {
};

class LsrInst final : public BinaryOperatorInst {
};

class ModInst final : public BinaryOperatorInst {
};

class MulInst final : public BinaryOperatorInst {
};

class MulhInst final : public BinaryOperatorInst {
};

class OrInst final : public BinaryOperatorInst {
};

class RemInst final : public BinaryOperatorInst {
};

class RotlInst final : public BinaryOperatorInst {
};

class ShlInst final : public BinaryOperatorInst {
};

class SraInst final : public BinaryOperatorInst {
};

class SrlInst final : public BinaryOperatorInst {
};

class SubInst final : public BinaryOperatorInst {
};

class XorInst final : public BinaryOperatorInst {
};
