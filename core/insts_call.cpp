// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts_call.h"



// -----------------------------------------------------------------------------
template<typename T>
CallSite<T>::CallSite(
    Inst::Kind kind,
    unsigned numOps,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv conv,
    const std::optional<Type> &type,
    uint64_t annot)
  : T(kind, numOps, annot)
  , numArgs_(args.size())
  , numFixed_(numFixed)
  , conv_(conv)
  , type_(type)
{
  this->template Op<0>() = callee;
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    *(this->op_begin() + i + 1) = args[i];
  }
}

// -----------------------------------------------------------------------------
CallInst::CallInst(
    std::optional<Type> type,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv conv,
    uint64_t annot)
  : CallSite(
        Inst::Kind::CALL,
        args.size() + 1,
        callee,
        args,
        numFixed,
        conv,
        type,
        annot
    )
{
}

// -----------------------------------------------------------------------------
TailCallInst::TailCallInst(
    std::optional<Type> type,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv conv,
    uint64_t annot)
  : CallSite(
        Kind::TCALL,
        args.size() + 1,
        callee,
        args,
        numFixed,
        conv,
        type,
        annot
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
    std::optional<Type> type,
    Inst *callee,
    const std::vector<Inst *> &args,
    Block *jcont,
    Block *jthrow,
    unsigned numFixed,
    CallingConv conv,
    uint64_t annot)
  : CallSite(
        Kind::INVOKE,
        args.size() + 3,
        callee,
        args,
        numFixed,
        conv,
        type,
        annot
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

// -----------------------------------------------------------------------------
TailInvokeInst::TailInvokeInst(
    Inst *callee,
    const std::vector<Inst *> &args,
    Block *jthrow,
    unsigned numFixed,
    CallingConv conv,
    uint64_t annot)
  : CallSite(
        Kind::TINVOKE,
        args.size() + 2,
        callee,
        args,
        numFixed,
        conv,
        std::nullopt,
        annot
    )
{
  Op<-1>() = jthrow;
}

// -----------------------------------------------------------------------------
TailInvokeInst::TailInvokeInst(
    Type type,
    Inst *callee,
    const std::vector<Inst *> &args,
    Block *jthrow,
    unsigned numFixed,
    CallingConv conv,
    uint64_t annot)
  : CallSite(
        Kind::TINVOKE,
        args.size() + 2,
        callee,
        args,
        numFixed,
        conv,
        std::optional<Type>(type),
        annot
    )
{
  Op<-1>() = jthrow;
}

// -----------------------------------------------------------------------------
Block *TailInvokeInst::getSuccessor(unsigned i) const
{
  if (i == 0) { return static_cast<Block *>(Op<-1>().get()); }
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned TailInvokeInst::getNumSuccessors() const
{
  return 1;
}
