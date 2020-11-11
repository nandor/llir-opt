// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"



/**
 * RISC-V Load-Linked/Store Conditional
 */
class RISCV_LL final : public MemoryInst {
public:
  RISCV_LL(
      Type type,
      Ref<Inst> addr,
      AnnotSet &&annot
  );

  /// Returns the number of return values.
  unsigned GetNumRets() const override { return 1; }
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }

  /// Returns the address.
  ConstRef<Inst> GetAddr() const;
  /// Returns the address.
  Ref<Inst> GetAddr();

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }

private:
  /// Type of the instruction.
  Type type_;
};

/**
 * RISCV Load-Linked/Store Conditional
 */
class RISCV_SC final : public MemoryInst {
public:
  RISCV_SC(
      Type type,
      Ref<Inst> addr,
      Ref<Inst> val,
      AnnotSet &&annot
  );

  /// Returns the number of return values.
  unsigned GetNumRets() const override { return 1; }
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }

  /// Returns the address.
  ConstRef<Inst> GetAddr() const;
  /// Returns the address.
  Ref<Inst> GetAddr();

  /// Returns the value.
  ConstRef<Inst> GetValue() const;
  /// Returns the value.
  Ref<Inst> GetValue();

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return true; }

private:
  /// Type of the instruction.
  Type type_;
};


/**
 * RISCV fence
 */
class RISCV_FENCE final : public Inst {
public:
  RISCV_FENCE(AnnotSet &&annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override { return 0; }
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return true; }
  /// Not a return.
  bool IsReturn() const override { return false; }
  /// Checks if the instruction is constant.
  bool IsConstant() const override { return false; }
};
