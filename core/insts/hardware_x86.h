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
  X86_XchgInst(Type type, Ref<Inst> addr, Ref<Inst> val, AnnotSet &&annot);

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
 * FPU control instruction class.
 */
class X86_FPUControlInst : public MemoryInst {
public:
  X86_FPUControlInst(Kind kind, Ref<Inst> addr, AnnotSet &&annot);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the pointer to the frame.
  ConstRef<Inst> GetAddr() const;
  /// Returns the pointer to the frame.
  Ref<Inst> GetAddr();

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
  X86_FnStCwInst(Ref<Inst> addr, AnnotSet &&annot);
};

/**
 * Stores the FPU environment into memory.
 */
class X86_FnStSwInst final : public X86_FPUControlInst {
public:
  X86_FnStSwInst(Ref<Inst> addr, AnnotSet &&annot);
};

/**
 * FnStEnvInst
 */
class X86_FnStEnvInst final : public X86_FPUControlInst {
public:
  X86_FnStEnvInst(Ref<Inst> addr, AnnotSet &&annot);
};

/**
 * FLdCwInst
 */
class X86_FLdCwInst final : public X86_FPUControlInst {
public:
  X86_FLdCwInst(Ref<Inst> addr, AnnotSet &&annot);
};

/**
 * FLdEnv
 */
class X86_FLdEnvInst final : public X86_FPUControlInst {
public:
  X86_FLdEnvInst(Ref<Inst> addr, AnnotSet &&annot);
};

/**
 * X86_LdmXCSR
 */
class X86_LdmXCSRInst final : public X86_FPUControlInst {
public:
  X86_LdmXCSRInst(Ref<Inst> addr, AnnotSet &&annot);
};

/**
 * X86_StmXCSR
 */
class X86_StmXCSRInst final : public X86_FPUControlInst {
public:
  X86_StmXCSRInst(Ref<Inst> addr, AnnotSet &&annot);
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

/**
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
 * X86 mfence barrier
 */
class X86_MFenceInst final : public MemoryInst {
public:
  X86_MFenceInst(AnnotSet &&annot);

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

/**
 * X86 CPUID
 */
class X86_CPUIDInst final : public Inst {
public:
  using type_iterator = std::vector<Type>::iterator;
  using const_type_iterator = std::vector<Type>::const_iterator;

  using type_range = llvm::iterator_range<type_iterator>;
  using const_type_range = llvm::iterator_range<const_type_iterator>;

public:
  X86_CPUIDInst(
      llvm::ArrayRef<Type> types,
      Ref<Inst> leaf,
      AnnotSet &&annot
  );
  X86_CPUIDInst(
      llvm::ArrayRef<Type> types,
      Ref<Inst> leaf,
      Ref<Inst> subleaf,
      AnnotSet &&annot
  );

  /// Returns the value.
  ConstRef<Inst> GetLeaf() const;
  /// Returns the value.
  Ref<Inst> GetLeaf();

  /// Returns the comparison reference.
  ConstRef<Inst> GetSubleaf() const;
  /// Returns the comparison reference.
  Ref<Inst> GetSubleaf();
  /// Check if a subleaf argument is present.
  bool HasSubleaf() const;

  /// Returns the number of return values.
  unsigned GetNumRets() const override { return types_.size(); }
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override { return types_[i]; }

  /// Iterators over types.
  size_t type_size() const { return types_.size(); }
  /// Check whether the function returns any values.
  bool type_empty() const { return types_.empty(); }
  /// Accessor to a given type.
  Type type(unsigned i) const { return types_[i]; }
  /// Start of type list.
  type_iterator type_begin() { return types_.begin(); }
  const_type_iterator type_begin() const { return types_.begin(); }
  /// End of type list.
  type_iterator type_end() { return types_.end(); }
  const_type_iterator type_end() const { return types_.end(); }
    /// Range of types.
  type_range types() { return llvm::make_range(type_begin(), type_end()); }
  const_type_range types() const { return llvm::make_range(type_begin(), type_end()); }

  /// This instruction has no side effects.
  bool HasSideEffects() const override { return false; }
  /// Not a return.
  bool IsReturn() const override { return false; }
  /// Checks if the instruction is constant.
  bool IsConstant() const override { return true; }

private:
  /// Returned types.
  std::vector<Type> types_;
};
