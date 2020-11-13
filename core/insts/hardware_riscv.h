// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"



/**
 * RISCV_CmpXchgInst
 */
class RISCV_CmpXchgInst final : public MemoryInst {
public:
  RISCV_CmpXchgInst(
      Type type,
      Ref<Inst> addr,
      Ref<Inst> val,
      Ref<Inst> ref,
      AnnotSet &&annot
  );

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }

  /// Returns the address.
  ConstRef<Inst> GetAddr() const;
  /// Returns the address.
  Ref<Inst> GetAddr();

  /// Returns the value.
  ConstRef<Inst> GetVal() const;
  /// Returns the value.
  Ref<Inst> GetVal();

  /// Returns the comparison reference.
  ConstRef<Inst> GetRef() const;
  /// Returns the comparison reference.
  Ref<Inst> GetRef();

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }

private:
  /// Type of the instruction.
  Type type_;
};

/**
 * RISCV fence
 */
class RISCV_FenceInst final : public Inst {
public:
  RISCV_FenceInst(AnnotSet &&annot);

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
