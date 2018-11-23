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
  SelectInst(
      Block *block,
      Type type,
      Inst *cond,
      Inst *vt,
      Inst *vf
  );

  Inst *GetCond() const { return static_cast<Inst *>(Op<0>().get()); }
  Inst *GetTrue() const { return static_cast<Inst *>(Op<1>().get()); }
  Inst *GetFalse() const { return static_cast<Inst *>(Op<2>().get()); }
};

/**
 * SetInst
 */
class SetInst final : public Inst {
public:
  SetInst(Block *block, ConstantReg *reg, Inst *val);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the value to set.
  ConstantReg *GetReg() const { return static_cast<ConstantReg *>(Op<0>().get()); }
  /// Returns the value to assign.
  Inst *GetValue() const { return static_cast<Inst *>(Op<1>().get()); }

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

  Value *GetArg() const { return static_cast<Value *>(Op<0>().get()); }
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
 * FrameInst
 */
class FrameInst final : public OperatorInst {
public:
  FrameInst(Block *block, Type type, ConstantInt *index);

  /// Returns the index.
  unsigned GetIdx() const;
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



#include "core/insts_binary.h"
#include "core/insts_call.h"
#include "core/insts_control.h"
#include "core/insts_memory.h"
#include "core/insts_unary.h"

