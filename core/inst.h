// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once



/**
 * Data Types known to the IR.
 */
enum class Type {
  I8, I16, I32, I64,
  U8, U16, U32, U64,
  F32, F64,
};


/**
 * Operand to an instruction.
 */
class Operand {
public:

private:

};



/**
 * Basic instruction.
 */
class Inst {
public:
  enum class Type {
    // Control flow.
    CALL, JT, JF, JI, JMP, RET, TCALL, SWITCH,
    // Memory.
    LD, ST, PUSH, POP,
    // Atomic.
    ATOMIC,
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

private:

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
};

class BinaryOperatorInst : public OperatorInst {
public:
};
