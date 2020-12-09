// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"


/**
 * Syscall
 */
class SyscallInst final : public Inst {
public:
  using arg_iterator = conv_op_iterator<Inst>;
  using arg_range = conv_op_range<Inst>;
  using const_arg_iterator = const_conv_op_iterator<Inst>;
  using const_arg_range = const_conv_op_range<Inst>;

  using type_iterator = std::vector<Type>::iterator;
  using const_type_iterator = std::vector<Type>::const_iterator;

  using type_range = llvm::iterator_range<type_iterator>;
  using const_type_range = llvm::iterator_range<const_type_iterator>;

public:
  SyscallInst(
      llvm::ArrayRef<Type> types,
      Ref<Inst> sysno,
      const std::vector<Ref<Inst> > &args,
      AnnotSet &&annot
  );
  SyscallInst(
      std::optional<Type> type,
      Ref<Inst> sysno,
      const std::vector<Ref<Inst> > &args,
      AnnotSet &&annot
  );

  SyscallInst(
      Ref<Inst> sysno,
      const std::vector<Ref<Inst> > &args,
      AnnotSet &&annot)
    : SyscallInst(std::nullopt, sysno, args, std::move(annot))
  {
  }

  SyscallInst(
      Type type,
      Ref<Inst> sysno,
      const std::vector<Ref<Inst> > &args,
      AnnotSet &&annot)
    : SyscallInst(std::optional<Type>(type), sysno, args, std::move(annot))
  {
  }

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns the type of the return value.
  std::optional<Type> GetType() const
  {
    return types_.size() > 0 ? std::optional<Type>(types_[0]) : std::nullopt;
  }

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

  /// Returns the syscall number.
  ConstRef<Inst> GetSyscall() const;
  /// Returns the syscall number.
  Ref<Inst> GetSyscall();

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }

  /// Return the number of arguments.
  size_t arg_size() const { return numOps_ - 1; }
  /// Returns the ith argument.
  ConstRef<Inst> arg(unsigned i) const;
  /// Returns the ith argument.
  Ref<Inst> arg(unsigned i);
  /// Start of the argument list.
  arg_iterator arg_begin() { return arg_iterator(this->value_op_begin() + 1); }
  /// End of the argument list.
  arg_iterator arg_end() { return arg_iterator(this->value_op_begin() + size()); }
  /// Range of arguments.
  arg_range args() { return llvm::make_range(arg_begin(), arg_end()); }

  /// Start of the argument list.
  const_arg_iterator arg_begin() const { return const_arg_iterator(this->value_op_begin() + 1); }
  /// End of the argument list.
  const_arg_iterator arg_end() const { return const_arg_iterator(this->value_op_begin() + size()); }
  /// Range of arguments.
  const_arg_range args() const { return llvm::make_range(arg_begin(), arg_end()); }

private:
  std::vector<Type> types_;
};
