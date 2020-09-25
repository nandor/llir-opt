// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/insts.h"



/**
 * LoadInst
 */
class LoadInst final : public MemoryInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::LD;

public:
  LoadInst(Type type, Value *addr, AnnotSet &&annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }
  /// Returns the address instruction.
  Inst *GetAddr() const;

  /// This instructions has no side effects.
  bool HasSideEffects() const override { return false; }

private:
  /// Type of the instruction.
  Type type_;
};

/**
 * StoreInst
 */
class StoreInst final : public MemoryInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::ST;

public:
  StoreInst(Inst *addr, Inst *val, AnnotSet &&annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the address to store the value at.
  Inst *GetAddr() const;
  /// Returns the value to store.
  Inst *GetVal() const;

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
};

/**
 * XchgInst
 */
class XchgInst final : public MemoryInst {
public:
  XchgInst(Type type, Inst *addr, Inst *val, AnnotSet &&annot);

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
 * CmpXchgInst
 */
class CmpXchgInst final : public MemoryInst {
public:
  CmpXchgInst(
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
 * VAStartInst
 */
class VAStartInst final : public Inst {
public:
  VAStartInst(Inst *vaList, AnnotSet &&annot);

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
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * AllocaInst
 */
class AllocaInst final : public OperatorInst {
public:
  AllocaInst(
      Type type,
      Inst *size,
      ConstantInt *align,
      AnnotSet &&annot
  );

  /// Returns the instruction size.
  Inst *GetCount() const { return static_cast<Inst *>(Op<0>().get()); }

  /// Returns the instruction alignment.
  int GetAlign() const
  {
    return static_cast<ConstantInt *>(Op<1>().get())->GetInt();
  }

  /// Instruction is constant if argument is.
  bool IsConstant() const override { return false; }
};
