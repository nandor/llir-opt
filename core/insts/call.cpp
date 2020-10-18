// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/insts/call.h"
#include "core/insts/const.h"



// -----------------------------------------------------------------------------
CallSite::CallSite(
    Inst::Kind kind,
    unsigned numOps,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv conv,
    llvm::ArrayRef<Type> types,
    AnnotSet &&annot)
  : TerminatorInst(kind, numOps, std::move(annot))
  , numArgs_(args.size())
  , numFixed_(numFixed)
  , conv_(conv)
  , types_(types)
{
  this->template Op<0>() = callee;
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    *(this->op_begin() + i + 1) = args[i];
  }
}

// -----------------------------------------------------------------------------
CallSite::CallSite(
    Inst::Kind kind,
    unsigned numOps,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv conv,
    llvm::ArrayRef<Type> types,
    const AnnotSet &annot)
  : TerminatorInst(kind, numOps, annot)
  , numArgs_(args.size())
  , numFixed_(numFixed)
  , conv_(conv)
  , types_(types)
{
  this->template Op<0>() = callee;
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    *(this->op_begin() + i + 1) = args[i];
  }
}

// -----------------------------------------------------------------------------
CallInst::CallInst(
    llvm::ArrayRef<Type> types,
    Inst *callee,
    const std::vector<Inst *> &args,
    Block *cont,
    unsigned numFixed,
    CallingConv conv,
    AnnotSet &&annot)
  : CallSite(
        Inst::Kind::CALL,
        args.size() + 2,
        callee,
        args,
        numFixed,
        conv,
        types,
        std::move(annot)
    )
{
  Op<-1>() = cont;
}

// -----------------------------------------------------------------------------
CallInst::CallInst(
    llvm::ArrayRef<Type> types,
    Inst *callee,
    const std::vector<Inst *> &args,
    Block *cont,
    unsigned numFixed,
    CallingConv conv,
    const AnnotSet &annot)
  : CallSite(
        Inst::Kind::CALL,
        args.size() + 2,
        callee,
        args,
        numFixed,
        conv,
        types,
        std::move(annot)
    )
{
  Op<-1>() = cont;
}

// -----------------------------------------------------------------------------
CallInst::~CallInst()
{
}

// -----------------------------------------------------------------------------
Block *CallInst::getSuccessor(unsigned i) const
{
  return static_cast<Block *>(Op<-1>().get());
}

// -----------------------------------------------------------------------------
TailCallInst::TailCallInst(
    llvm::ArrayRef<Type> types,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv conv,
    AnnotSet &&annot)
  : CallSite(
        Kind::TCALL,
        args.size() + 1,
        callee,
        args,
        numFixed,
        conv,
        types,
        std::move(annot)
    )
{
}

// -----------------------------------------------------------------------------
TailCallInst::TailCallInst(
    llvm::ArrayRef<Type> types,
    Inst *callee,
    const std::vector<Inst *> &args,
    unsigned numFixed,
    CallingConv conv,
    const AnnotSet &annot)
  : CallSite(
        Kind::TCALL,
        args.size() + 1,
        callee,
        args,
        numFixed,
        conv,
        types,
        annot
    )
{
}

// -----------------------------------------------------------------------------
Block *TailCallInst::getSuccessor(unsigned i) const
{
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned TailCallInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
InvokeInst::InvokeInst(
    llvm::ArrayRef<Type> types,
    Inst *callee,
    const std::vector<Inst *> &args,
    Block *jcont,
    Block *jthrow,
    unsigned numFixed,
    CallingConv conv,
    AnnotSet &&annot)
  : CallSite(
        Kind::INVOKE,
        args.size() + 3,
        callee,
        args,
        numFixed,
        conv,
        types,
        std::move(annot)
    )
{
  Op<-2>() = jcont;
  Op<-1>() = jthrow;
}

// -----------------------------------------------------------------------------
InvokeInst::InvokeInst(
    llvm::ArrayRef<Type> types,
    Inst *callee,
    const std::vector<Inst *> &args,
    Block *jcont,
    Block *jthrow,
    unsigned numFixed,
    CallingConv conv,
    const AnnotSet &annot)
  : CallSite(
        Kind::INVOKE,
        args.size() + 3,
        callee,
        args,
        numFixed,
        conv,
        types,
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
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned InvokeInst::getNumSuccessors() const
{
  return 2;
}

// -----------------------------------------------------------------------------
Inst *GetCalledInst(Inst *inst)
{
  switch (inst->GetKind()) {
    case Inst::Kind::CALL: {
      return static_cast<CallInst *>(inst)->GetCallee();
    }
    case Inst::Kind::INVOKE: {
      return static_cast<InvokeInst *>(inst)->GetCallee();
    }
    case Inst::Kind::TCALL: {
      return static_cast<TailCallInst *>(inst)->GetCallee();
    }
    default: {
      return nullptr;
    }
  }
}

// -----------------------------------------------------------------------------
Func *GetCallee(Inst *inst)
{
  Inst *callee = nullptr;
  switch (inst->GetKind()) {
    case Inst::Kind::CALL: {
      callee = static_cast<CallInst *>(inst)->GetCallee();
      break;
    }
    case Inst::Kind::INVOKE: {
      callee = static_cast<InvokeInst *>(inst)->GetCallee();
      break;
    }
    case Inst::Kind::TCALL: {
      callee = static_cast<TailCallInst *>(inst)->GetCallee();
      break;
    }
    default: {
      return nullptr;
    }
  }

  if (auto *movInst = ::dyn_cast_or_null<MovInst>(callee)) {
    return ::dyn_cast_or_null<Func>(movInst->GetArg());
  } else {
    return nullptr;
  }
}
