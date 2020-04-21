// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"



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
 * RdtscInst
 */
class RdtscInst final : public OperatorInst {
public:
  RdtscInst(Type type, const AnnotSet &annot);

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

/**
 * FNStCwInst
 */
class FNStCwInst final : public Inst {
public:
  FNStCwInst(Inst *addr, const AnnotSet &annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the pointer to the frame.
  Inst *GetAddr() const { return static_cast<Inst *>(Op<0>().get()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};


/**
 * FLdCwInst
 */
class FLdCwInst final : public Inst {
public:
  FLdCwInst(Inst *addr, const AnnotSet &annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the pointer to the frame.
  Inst *GetAddr() const { return static_cast<Inst *>(Op<0>().get()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};

