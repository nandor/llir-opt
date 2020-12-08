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
  LoadInst(Type type, Ref<Inst> addr, AnnotSet &&annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }

  /// Returns the address instruction.
  ConstRef<Inst> GetAddr() const;
  /// Returns the address instruction.
  Ref<Inst> GetAddr();

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
  StoreInst(Ref<Inst> addr, Ref<Inst> val, AnnotSet &&annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the address to store the value at.
  ConstRef<Inst> GetAddr() const;
  /// Returns the address to store the value at.
  Ref<Inst> GetAddr();

  /// Returns the value to store.
  ConstRef<Inst> GetVal() const;
  /// Returns the value to store.
  Ref<Inst> GetVal();

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
};

/**
 * VAStartInst
 */
class VAStartInst final : public Inst {
public:
  VAStartInst(Ref<Inst> vaList, AnnotSet &&annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the pointer to the frame.
  ConstRef<Inst> GetVAList() const;
  /// Returns the pointer to the frame.
  Ref<Inst> GetVAList();

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
      Ref<Inst> size,
      unsigned align,
      AnnotSet &&annot
  );
  AllocaInst(
      Type type,
      Ref<Inst> size,
      ConstantInt *align,
      AnnotSet &&annot
  );

  /// Returns the instruction size.
  ConstRef<Inst> GetCount() const;
  /// Returns the instruction size.
  Ref<Inst> GetCount();

  /// Returns the instruction alignment.
  int GetAlign() const;

  /// Instruction is constant if argument is.
  bool IsConstant() const override { return false; }
};
