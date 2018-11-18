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
      Block *parent,
      unsigned numOps,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv
  );

  /// Checks if the function is var arg: more args than fixed ones.
  bool IsVarArg() const { return numArgs_ > numFixed_; }

  /// Returns the number of fixed arguments.
  unsigned GetNumFixedArgs() const { return numFixed_; }

  /// Returns the number of arguments.
  unsigned GetNumArgs() const
  {
    return arg_end() - arg_begin();
  }

  /// Returns the calling convention.
  CallingConv GetCallingConv() const { return callConv_; }
  /// Returns the number of fixed arguments, i.e. the size of the call.
  virtual std::optional<size_t> GetSize() const { return numFixed_; }

  /// Start of the argument list.
  const_arg_iterator arg_begin() const
  {
    return const_arg_iterator(this->op_begin() + 1);
  }

  /// End of the argument list.
  const_arg_iterator arg_end() const
  {
    return const_arg_iterator(this->op_end());
  }

  /// Range of arguments.
  const_arg_range args() const
  {
    return llvm::make_range(arg_begin(), arg_end());
  }

  /// Returns the callee.
  Inst *GetCallee() const
  {
    return static_cast<Inst *>(this->template Op<0>().get());
  }

private:
  /// Number of actual arguments.
  unsigned numArgs_;
  /// Number of fixed arguments.
  unsigned numFixed_;
  /// Calling convention of the call.
  CallingConv callConv_;
};


/**
 * CallInst
 */
class CallInst final : public CallSite<ControlInst> {
public:
  /// Creates a void call.
  CallInst(
      Block *block,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv)
    : CallInst(
          block,
          std::nullopt,
          callee,
          args,
          numFixed,
          callConv
      )
  {
  }

  /// Creates a call returning a value.
  CallInst(
      Block *block,
      Type type,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv)
    : CallInst(
          block,
          std::optional<Type>(type),
          callee,
          args,
          numFixed,
          callConv
      )
  {
  }

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns the callee.
  Inst *GetCallee() const { return static_cast<Inst *>(Op<0>().get()); }

private:
  /// Initialises the call instruction.
  CallInst(
      Block *block,
      std::optional<Type> type,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv
  );

private:
  /// Returns the type of the return value.
  std::optional<Type> type_;
};

/**
 * TailCallInst
 */
class TailCallInst final : public CallSite<TerminatorInst> {
public:
  TailCallInst(
      Block *block,
      Inst *callee,
      const std::vector<Value *> &args,
      unsigned numFixed,
      CallingConv callConv
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
      Block *block,
      Inst *callee,
      const std::vector<Value *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv)
    : InvokeInst(
          block,
          std::nullopt,
          callee,
          args,
          jcont,
          jthrow,
          numFixed,
          callConv
      )
  {
  }

  InvokeInst(
      Block *block,
      Type type,
      Inst *callee,
      const std::vector<Value *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv)
    : InvokeInst(
          block,
          std::optional<Type>(type),
          callee,
          args,
          jcont,
          jthrow,
          numFixed,
          callConv
      )
  {
  }

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

private:
  /// Initialises the invoke instruction.
  InvokeInst(
      Block *block,
      std::optional<Type> type,
      Inst *callee,
      const std::vector<Value *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv callConv
  );

private:
  /// Returns the type of the return value.
  std::optional<Type> type_;
};
