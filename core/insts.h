// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <optional>

#include <llvm/ADT/iterator.h>

#include "inst.h"



/**
 * SelectInst
 */
class SelectInst final : public OperatorInst {
public:
  SelectInst(Type type, Inst *cond, Inst *vt, Inst *vf, const AnnotSet &annot);

  Inst *GetCond() const { return static_cast<Inst *>(Op<0>().get()); }
  Inst *GetTrue() const { return static_cast<Inst *>(Op<1>().get()); }
  Inst *GetFalse() const { return static_cast<Inst *>(Op<2>().get()); }

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

/**
 * SetInst
 */
class SetInst final : public Inst {
public:
  SetInst(ConstantReg *reg, Inst *val, const AnnotSet &annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the value to set.
  ConstantReg *GetReg() const { return static_cast<ConstantReg *>(Op<0>().get()); }
  /// Returns the value to assign.
  Inst *GetValue() const { return static_cast<Inst *>(Op<1>().get()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

/**
 * MovInst
 */
class MovInst final : public OperatorInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::MOV;

public:
  MovInst(Type type, Value *op, const AnnotSet &annot = {})
    : OperatorInst(Kind::MOV, type, 1, annot)
  {
    Op<0>() = op;
  }

  Value *GetArg() const { return static_cast<Value *>(Op<0>().get()); }

  /// Instruction is constant if argument is.
  bool IsConstant() const override { return !GetArg()->Is(Value::Kind::INST); }
};

/**
 * ArgInst
 */
class ArgInst final : public ConstInst {
public:
  ArgInst(Type type, ConstantInt *index, const AnnotSet &annot = {});

  /// Returns the argument index.
  unsigned GetIdx() const;

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

/**
 * FrameInst
 */
class FrameInst final : public ConstInst {
public:
  FrameInst(Type type, ConstantInt *index, const AnnotSet &annot = {});

  /// Returns the index.
  unsigned GetIdx() const;

  /// Instruction is constant.
  bool IsConstant() const override { return true; }
};

/**
 * VAStartInst
 */
class VAStartInst final : public Inst {
public:
  VAStartInst(Inst *vaList, const AnnotSet &annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the pointer to the frame.
  Inst *GetVAList() const { return static_cast<Inst *>(Op<0>().get()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

/**
 * Undefined value.
 */
class UndefInst final : public ConstInst {
public:
  UndefInst(Type type, const AnnotSet &annot = {});

  /// Instruction is constant.
  bool IsConstant() const override { return true; }
};

/**
 * PHI instruction.
 */
class PhiInst final : public Inst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::PHI;

public:
  PhiInst(Type type, const AnnotSet &annot = {});

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

  /// Updates the nth block.
  void SetBlock(unsigned i, Block *block);

  /// Returns the immediate type.
  Type GetType() const { return type_; }
  /// Checks if the PHI has a value for a block.
  bool HasValue(const Block *block) const;
  /// Returns an operand for a block.
  Value *GetValue(const Block *block) const;

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }

private:
  /// Type of the PHI node.
  Type type_;
};



#include "core/insts_binary.h"
#include "core/insts_call.h"
#include "core/insts_control.h"
#include "core/insts_memory.h"
#include "core/insts_unary.h"

