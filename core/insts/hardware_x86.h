// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"



/**
 * X86_XchgInst
 *
 * Atomically stores the value into the memory location and returns the
 * prior value from memory.
 */
class X86_XchgInst final : public MemoryInst {
public:
  X86_XchgInst(Type type, Inst *addr, Inst *val, AnnotSet &&annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }
  /// Returns the address.
  Inst *GetAddr() const { return static_cast<Inst *>(Op<0>().get()); }
  /// Returns the value.
  Inst *GetVal() const { return static_cast<Inst *>(Op<1>().get()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }

private:
  /// Type of the instruction.
  Type type_;
};

/**
 * X86_CmpXchgInst
 */
class X86_CmpXchgInst final : public MemoryInst {
public:
  X86_CmpXchgInst(
      Type type,
      Inst *addr,
      Inst *val,
      Inst *ref,
      AnnotSet &&annot
  );

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }
  /// Returns the address.
  Inst *GetAddr() const { return static_cast<Inst *>(Op<0>().get()); }
  /// Returns the value.
  Inst *GetVal() const { return static_cast<Inst *>(Op<1>().get()); }
  /// Returns the comparison reference.
  Inst *GetRef() const { return static_cast<Inst *>(Op<2>().get()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }

private:
  /// Type of the instruction.
  Type type_;
};

/**
 * X86_RdtscInst
 *
 * Returns a 64-bit time stamp.
 */
class X86_RdtscInst final : public OperatorInst {
public:
  X86_RdtscInst(Type type, AnnotSet &&annot);

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * FPU control instruction class.
 */
class X86_FPUControlInst : public Inst {
public:
  X86_FPUControlInst(Kind kind, Inst *addr, AnnotSet &&annot);

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
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * Stores the FPU control word into memory.
 */
class X86_FnStCwInst final : public X86_FPUControlInst {
public:
  X86_FnStCwInst(Inst *addr, AnnotSet &&annot);
};

/**
 * Stores the FPU environment into memory.
 */
class X86_FnStSwInst final : public X86_FPUControlInst {
public:
  X86_FnStSwInst(Inst *addr, AnnotSet &&annot);
};

/**
 * FnStEnvInst
 */
class X86_FnStEnvInst final : public X86_FPUControlInst {
public:
  X86_FnStEnvInst(Inst *addr, AnnotSet &&annot);
};

/**
 * FLdCwInst
 */
class X86_FLdCwInst final : public X86_FPUControlInst {
public:
  X86_FLdCwInst(Inst *addr, AnnotSet &&annot);
};

/**
 * FLdEnv
 */
class X86_FLdEnvInst final : public X86_FPUControlInst {
public:
  X86_FLdEnvInst(Inst *addr, AnnotSet &&annot);
};

/**
 * X86_LdmXCSR
 */
class X86_LdmXCSRInst final : public X86_FPUControlInst {
public:
  X86_LdmXCSRInst(Inst *addr, AnnotSet &&annot);
};

/**
 * X86_StmXCSR
 */
class X86_StmXCSRInst final : public X86_FPUControlInst {
public:
  X86_StmXCSRInst(Inst *addr, AnnotSet &&annot);
};

/**
 * Clears FPU exceptions.
 */
class X86_FnClExInst : public Inst {
public:
  X86_FnClExInst(AnnotSet &&annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};
