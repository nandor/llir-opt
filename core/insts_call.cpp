// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts_call.h"



// -----------------------------------------------------------------------------
template<typename T>
CallSite<T>::CallSite(
    Inst::Kind kind,
    Block *parent,
    unsigned numOps,
    Inst *callee,
    const std::vector<Value *> &args,
    unsigned numFixed,
    CallingConv callConv)
  : T(kind, parent, numOps)
  , numArgs_(args.size())
  , numFixed_(numFixed)
  , callConv_(callConv)
{
  this->template Op<0>() = callee;
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    *(this->op_begin() + i + 1) = args[i];
  }
}

// -----------------------------------------------------------------------------
CallInst::CallInst(
    Block *block,
    std::optional<Type> type,
    Inst *callee,
    const std::vector<Value *> &args,
    unsigned numFixed,
    CallingConv callConv)
  : CallSite(
        Kind::CALL,
        block,
        args.size() + 1,
        callee,
        args,
        numFixed,
        callConv
    )
  , type_(type)
{
}

// -----------------------------------------------------------------------------
unsigned CallInst::GetNumRets() const
{
  return type_ ? 1 : 0;
}

// -----------------------------------------------------------------------------
Type CallInst::GetType(unsigned i) const
{
  if (i == 0 && type_) return *type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
TailCallInst::TailCallInst(
    Block *block,
    Inst *callee,
    const std::vector<Value *> &args,
    unsigned numFixed,
    CallingConv callConv)
  : CallSite(
        Kind::TCALL,
        block,
        args.size() + 1,
        callee,
        args,
        numFixed,
        callConv
    )
{
}

// -----------------------------------------------------------------------------
Block *TailCallInst::getSuccessor(unsigned i) const
{
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned TailCallInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
InvokeInst::InvokeInst(
    Block *block,
    std::optional<Type> type,
    Inst *callee,
    const std::vector<Value *> &args,
    Block *jcont,
    Block *jthrow,
    unsigned numFixed,
    CallingConv callConv)
  : CallSite(
        Kind::INVOKE,
        block,
        args.size() + 3,
        callee,
        args,
        numFixed,
        callConv
    )
  , type_(type)
{
  Op<-2>() = jcont;
  Op<-1>() = jthrow;
}

// -----------------------------------------------------------------------------
unsigned InvokeInst::GetNumRets() const
{
  return type_ ? 1 : 0;
}

// -----------------------------------------------------------------------------
Type InvokeInst::GetType(unsigned i) const
{
  if (i == 0 && type_) return *type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
Block *InvokeInst::getSuccessor(unsigned i) const
{
  if (i == 0) { return static_cast<Block *>(Op<-2>().get()); }
  if (i == 1) { return static_cast<Block *>(Op<-1>().get()); }
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned InvokeInst::getNumSuccessors() const
{
  return 2;
}
