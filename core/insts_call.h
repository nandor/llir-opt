// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/iterator.h>

#include "calling_conv.h"
#include "inst.h"



/**
 * Base class for call instructions.
 */
template<typename T>
class CallSite : public T {
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

    Inst *operator*() const
    {
      return static_cast<Inst *>(this->I->get());
    }

    Inst *operator->() const
    {
      return static_cast<Inst *>(this->I->get());
    }
  };

  class const_arg_iterator : public adapter<const_arg_iterator, User::const_op_iterator, const Inst> {
  public:
    explicit const_arg_iterator(User::const_op_iterator it)
      : adapter<const_arg_iterator, User::const_op_iterator, const Inst>(it)
    {
    }

    const Inst *operator*() const
    {
      return static_cast<const Inst *>(this->I->get());
    }

    const Inst *operator->() const
    {
      return static_cast<const Inst *>(this->I->get());
    }
  };

  using arg_range = llvm::iterator_range<arg_iterator>;
  using const_arg_range = llvm::iterator_range<const_arg_iterator>;

public:
  CallSite(
      Inst::Kind kind,
      unsigned numOps,
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv callConv,
      const std::optional<Type> &type,
      uint64_t annot
  );

  /// Checks if the function is var arg: more args than fixed ones.
  bool IsVarArg() const { return numArgs_ > numFixed_; }

  /// Returns the number of fixed arguments.
  unsigned GetNumFixedArgs() const { return numFixed_; }

  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return numArgs_; }

  /// Returns the calling convention.
  CallingConv GetCallingConv() const { return callConv_; }
  /// Returns the number of fixed arguments, i.e. the size of the call.
  virtual std::optional<size_t> GetSize() const override { return numFixed_; }

  /// Start of the argument list.
  arg_iterator arg_begin()
  {
    return arg_iterator(this->op_begin() + 1);
  }
  const_arg_iterator arg_begin() const
  {
    return const_arg_iterator(this->op_begin() + 1);
  }

  /// End of the argument list.
  arg_iterator arg_end()
  {
    return arg_iterator(this->op_begin() + 1 + numArgs_);
  }
  const_arg_iterator arg_end() const
  {
    return const_arg_iterator(this->op_begin() + 1 + numArgs_);
  }

  /// Range of arguments.
  arg_range args()
  {
    return llvm::make_range(arg_begin(), arg_end());
  }
  const_arg_range args() const
  {
    return llvm::make_range(arg_begin(), arg_end());
  }

  /// Returns the callee.
  Inst *GetCallee() const
  {
    return static_cast<Inst *>(this->template Op<0>().get());
  }

  /// Returns the number of return values.
  unsigned GetNumRets() const override
  {
    return type_ ? 1 : 0;
  }

  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override
  {
    if (i == 0 && type_) return *type_;
    throw InvalidOperandException();
  }

  /// Returns the type, if it exists.
  std::optional<Type> GetType() const
  {
    return type_;
  }

private:
  /// Number of actual arguments.
  unsigned numArgs_;
  /// Number of fixed arguments.
  unsigned numFixed_;
  /// Calling convention of the call.
  CallingConv callConv_;
  /// Returns the type of the return value.
  std::optional<Type> type_;
};


/**
 * CallInst
 */
class CallInst final : public CallSite<ControlInst> {
public:
  /// Creates a void call.
  CallInst(
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv callConv,
      uint64_t annot
  );

  /// Creates a call returning a value.
  CallInst(
      Type type,
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv callConv,
      uint64_t annot
  );
};

/**
 * TailCallInst
 */
class TailCallInst final : public CallSite<TerminatorInst> {
public:
  TailCallInst(
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv callConv,
      uint64_t annot
  );

  TailCallInst(
      Type type,
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv callConv,
      uint64_t annot
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;
};

/**
 * InvokeInst
 */
class InvokeInst final : public CallSite<TerminatorInst> {
public:
  InvokeInst(
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv,
      uint64_t annot
  );

  InvokeInst(
      Type type,
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv,
      uint64_t annot
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the continuation.
  Block *getCont() const { return getSuccessor(0); }
  /// Returns the landing pad.
  Block *getThrow() const { return getSuccessor(1); }
};

/**
 * TailInvokeInst
 */
class TailInvokeInst final : public CallSite<TerminatorInst> {
public:
  TailInvokeInst(
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv,
      uint64_t annot
  );

  TailInvokeInst(
      Type type,
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv,
      uint64_t annot
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;
  /// Returns the landing pad.
  Block *getThrow() const { return getSuccessor(0); }
};

