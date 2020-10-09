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
 * RdtscInst
 */
class RdtscInst final : public OperatorInst {
public:
  RdtscInst(Type type, AnnotSet &&annot);

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * FNStCwInst
 */
class FNStCwInst final : public Inst {
public:
  FNStCwInst(Inst *addr, AnnotSet &&annot);

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
 * FLdCwInst
 */
class FLdCwInst final : public Inst {
public:
  FLdCwInst(Inst *addr, AnnotSet &&annot);

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
 * Syscall
 */
class SyscallInst final : public Inst {
public:
  template<typename It, typename Jt, typename U>
  using adapter = llvm::iterator_adaptor_base
      < It
      , Jt
      , std::random_access_iterator_tag
      , U *
      , ptrdiff_t
      , U *
      , U *
      >;

  class arg_iterator : public adapter<arg_iterator, User::op_iterator, Inst> {
  public:
    explicit arg_iterator(User::op_iterator it)
      : adapter<arg_iterator, User::op_iterator, Inst>(it)
    {
    }

    Inst *operator*() const { return static_cast<Inst *>(this->I->get()); }
    Inst *operator->() const { return static_cast<Inst *>(this->I->get()); }
  };

  class const_arg_iterator : public adapter<const_arg_iterator, User::const_op_iterator, const Inst> {
  public:
    explicit const_arg_iterator(User::const_op_iterator it)
      : adapter<const_arg_iterator, User::const_op_iterator, const Inst>(it)
    {
    }

    const Inst *operator*() const { return static_cast<const Inst *>(this->I->get()); }
    const Inst *operator->() const { return static_cast<const Inst *>(this->I->get()); }
  };

  using arg_range = llvm::iterator_range<arg_iterator>;
  using const_arg_range = llvm::iterator_range<const_arg_iterator>;

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
