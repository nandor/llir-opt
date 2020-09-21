// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/iterator.h>

#include "core/calling_conv.h"
#include "core/inst.h"
#include "core/func.h"

class Func;



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
      CallingConv conv,
      const std::optional<Type> &type,
      AnnotSet annot
  );

  /// Checks if the function is var arg: more args than fixed ones.
  bool IsVarArg() const { return numArgs_ > numFixed_; }

  /// Returns the number of fixed arguments.
  unsigned GetNumFixedArgs() const { return numFixed_; }

  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return numArgs_; }

  /// Returns the calling convention.
  CallingConv GetCallingConv() const { return conv_; }
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

  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override
  {
    if (i == 0 && type_) return *type_;
    llvm_unreachable("invalid operand");
  }

  /// Returns the type, if it exists.
  std::optional<Type> GetType() const
  {
    return type_;
  }

  /// This instruction has side effects.
  bool HasSideEffects() const override { return true; }

  /// Changes the calling conv.
  void SetCallingConv(CallingConv conv) { conv_ = conv; }

protected:
  /// Number of actual arguments.
  unsigned numArgs_;
  /// Number of fixed arguments.
  unsigned numFixed_;
  /// Calling convention of the call.
  CallingConv conv_;
  /// Returns the type of the return value.
  std::optional<Type> type_;
};


/**
 * CallInst
 */
class CallInst final : public CallSite<ControlInst> {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::CALL;

public:
  /// Creates a void call.
  CallInst(
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot)
    : CallInst(std::nullopt, callee, args, numFixed, conv, annot)
  {
  }

  /// Creates a call returning a value.
  CallInst(
      Type type,
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot)
    : CallInst(std::optional<Type>(type), callee, args, numFixed, conv, annot)
  {
  }

  /// Creates a call with an optional type.
  CallInst(
      std::optional<Type> type,
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot
  );

  /// Returns the number of return values.
  unsigned GetNumRets() const override { return type_ ? 1 : 0; }

  /// Instruction does not return.
  bool IsReturn() const override { return false; }
};

/**
 * TailCallInst
 */
class TailCallInst final : public CallSite<TerminatorInst> {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::TCALL;

public:
  TailCallInst(
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot)
    : TailCallInst(std::nullopt, callee, args, numFixed, conv, annot)
  {
  }

  TailCallInst(
      Type type,
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot)
    : TailCallInst(std::optional<Type>(type), callee, args, numFixed, conv, annot)
  {
  }

  TailCallInst(
      std::optional<Type> type,
      Inst *callee,
      const std::vector<Inst *> &args,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;
  /// Returns the number of return values.
  unsigned GetNumRets() const override { return 0; }
  /// Instruction returns.
  bool IsReturn() const override { return true; }
};

/**
 * InvokeInst
 */
class InvokeInst final : public CallSite<TerminatorInst> {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::INVOKE;

public:
  InvokeInst(
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot)
    : InvokeInst(
        std::nullopt,
        callee,
        args,
        jcont,
        jthrow,
        numFixed,
        conv,
        annot
      )
  {
  }

  InvokeInst(
      Type type,
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot)
    : InvokeInst(
        std::optional<Type>(type),
        callee,
        args,
        jcont,
        jthrow,
        numFixed,
        conv,
        annot
      )
  {
  }

  InvokeInst(
      std::optional<Type> type,
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jcont,
      Block *jthrow,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the continuation.
  Block *GetCont() const { return getSuccessor(0); }
  /// Returns the landing pad.
  Block *GetThrow() const { return getSuccessor(1); }
  /// Returns the number of return values.
  unsigned GetNumRets() const override { return type_ ? 1 : 0; }

  /// Instruction returns.
  bool IsReturn() const override { return true; }
};

/**
 * TailInvokeInst
 */
class TailInvokeInst final : public CallSite<TerminatorInst> {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::TINVOKE;

public:
  TailInvokeInst(
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jthrow,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot
  );

  TailInvokeInst(
      Type type,
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jthrow,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot
  );

  TailInvokeInst(
      std::optional<Type> type,
      Inst *callee,
      const std::vector<Inst *> &args,
      Block *jthrow,
      unsigned numFixed,
      CallingConv conv,
      AnnotSet annot
  );

  /// Returns the successor node.
  Block *getSuccessor(unsigned i) const override;
  /// Returns the number of successors.
  unsigned getNumSuccessors() const override;

  /// Returns the landing pad.
  Block *GetThrow() const { return getSuccessor(0); }
  /// Returns the number of return values.
  unsigned GetNumRets() const override { return 0; }

  /// Instruction returns.
  bool IsReturn() const override { return true; }
};

/**
 * Returns an instruction if used as the target of a call.
 */
Inst *GetCalledInst(Inst *inst);

/**
 * Returns the callee, if the instruction is a call with a mov as an argument.
 */
Func *GetCallee(Inst *inst);

/**
 * Returns the callee, if the instruction is a call with a mov as an argument.
 */
inline const Func *GetCallee(const Inst *inst)
{
  return GetCallee(const_cast<Inst *>(inst));
}
