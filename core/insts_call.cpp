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
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv callConv,
    const std::optional<Type> &type)
  : T(kind, parent, numOps)
  , numArgs_(args.size())
  , numFixed_(numFixed)
  , callConv_(callConv)
  , type_(type)
{
  this->template Op<0>() = callee;
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    *(this->op_begin() + i + 1) = args[i];
  }
}

// -----------------------------------------------------------------------------
CallInst::CallInst(
    Block *block,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv callConv)
  : CallSite(
        Inst::Kind::CALL,
        block,
        args.size() + 1,
        callee,
        args,
        numFixed,
        callConv,
        std::nullopt
    )
{
}

// -----------------------------------------------------------------------------
CallInst::CallInst(
    Block *block,
    Type type,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv callConv)
  : CallSite(
        Inst::Kind::CALL,
        block,
        args.size() + 1,
        callee,
        args,
        numFixed,
        callConv,
        std::optional<Type>(type)
    )
{
}

// -----------------------------------------------------------------------------
TailCallInst::TailCallInst(
    Block *block,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv callConv)
  : CallSite(
        Kind::TCALL,
        block,
        args.size() + 1,
        callee,
        args,
        numFixed,
        callConv,
        std::nullopt
    )
{
}

// -----------------------------------------------------------------------------
TailCallInst::TailCallInst(
    Block *block,
    Type type,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv callConv)
  : CallSite(
        Kind::TCALL,
        block,
        args.size() + 1,
        callee,
        args,
        numFixed,
        callConv,
        std::optional<Type>(type)
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
    Inst *callee,
    const std::vector<Inst *> &args,
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
        callConv,
        std::nullopt
    )
{
}

// -----------------------------------------------------------------------------
InvokeInst::InvokeInst(
    Block *block,
    Type type,
    Inst *callee,
    const std::vector<Inst *> &args,
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
        callConv,
        std::optional<Type>(type)
    )
{
  Op<-2>() = jcont;
  Op<-1>() = jthrow;
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
