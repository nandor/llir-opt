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
  SetInst(ConstantReg *reg, Inst *val, AnnotSet &&annot);

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
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * Syscall
 */
class SyscallInst final : public Inst {
public:
  SyscallInst(
      std::optional<Type> type,
      Inst *sysno,
      const std::vector<Inst *> &args,
      AnnotSet &&annot
  );

  SyscallInst(
      Inst *sysno,
      const std::vector<Inst *> &args,
      AnnotSet &&annot)
    : SyscallInst(std::nullopt, sysno, args, std::move(annot))
  {
  }

  SyscallInst(
      Type type,
      Inst *sysno,
      const std::vector<Inst *> &args,
      AnnotSet &&annot)
    : SyscallInst(std::optional<Type>(type), sysno, args, std::move(annot))
  {
  }

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns the type of the return value.
  std::optional<Type> GetType() const { return type_; }

  /// Returns the syscall number.
  Inst *GetSyscall() const { return static_cast<Inst *>(Op<0>().get()); }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }

  /// Start of the argument list.
  arg_iterator arg_begin() { return arg_iterator(this->op_begin() + 1); }
  /// End of the argument list.
  arg_iterator arg_end() { return arg_iterator(this->op_begin() + size()); }
  /// Range of arguments.
  arg_range args() { return llvm::make_range(arg_begin(), arg_end()); }

  /// Start of the argument list.
  const_arg_iterator arg_begin() const { return const_arg_iterator(this->op_begin() + 1); }
  /// End of the argument list.
  const_arg_iterator arg_end() const { return const_arg_iterator(this->op_begin() + size()); }
  /// Range of arguments.
  const_arg_range args() const { return llvm::make_range(arg_begin(), arg_end()); }

private:
  std::optional<Type> type_;
};

/**
 * Wrapper around the clone system call.
 */
class CloneInst final : public ControlInst {
public:
  /// Creates a new clone instruction.
  CloneInst(
      Type type,
      Inst *callee,
      Inst *stack,
      Inst *flag,
      Inst *arg,
      Inst *tpid,
      Inst *tls,
      Inst *ctid,
      AnnotSet &&annot
    );

  /// Return the callee.
  const Inst *GetCallee() const { return static_cast<Inst *>(Op<0>().get()); }
  /// Return the stack of the new thread.
  const Inst *GetStack() const { return static_cast<Inst *>(Op<1>().get()); }
  /// Return the clone flags.
  const Inst *GetFlags() const { return static_cast<Inst *>(Op<2>().get()); }
  /// Return the argument to the thread.
  const Inst *GetArg() const { return static_cast<Inst *>(Op<3>().get()); }
  /// Return the parent thread ID.
  const Inst *GetPTID() const { return static_cast<Inst *>(Op<4>().get()); }
  /// Return the thread descriptor.
  const Inst *GetTLS() const { return static_cast<Inst *>(Op<5>().get()); }
  /// Return the child PID.
  const Inst *GetCTID() const { return static_cast<Inst *>(Op<6>().get()); }

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns the instruction type.
  Type GetType() const { return type_; }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }
  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }

private:
  /// Type of the clone return value.
  Type type_;
};
