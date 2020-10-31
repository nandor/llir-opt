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
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
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
  Set<0>(callee);
  for (unsigned i = 0, n = args.size(); i < n; ++i){
    Set(i + 1, args[i]);
  }
}

// -----------------------------------------------------------------------------
CallSite::CallSite(
    Inst::Kind kind,
    unsigned numOps,
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
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
  Set<0>(callee);
  for (unsigned i = 0, n = args.size(); i < n; ++i){
    Set(i + 1, args[i]);
  }
}

// -----------------------------------------------------------------------------
ConstRef<Inst> CallSite::GetCallee() const
{
  return cast<Inst>(static_cast<ConstRef<Value>>(Get<0>()));
}

// -----------------------------------------------------------------------------
Ref<Inst> CallSite::GetCallee()
{
  return cast<Inst>(static_cast<Ref<Value>>(Get<0>()));
}

// -----------------------------------------------------------------------------
ConstRef<Inst> CallSite::arg(unsigned i) const
{
  return cast<Inst>(static_cast<ConstRef<Value>>(Get(i + 1)));
}

// -----------------------------------------------------------------------------
Ref<Inst> CallSite::arg(unsigned i)
{
  return cast<Inst>(static_cast<Ref<Value>>(Get(i + 1)));
}

// -----------------------------------------------------------------------------
CallInst::CallInst(
    llvm::ArrayRef<Type> types,
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
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
  Set<-1>(cont);
}

// -----------------------------------------------------------------------------
CallInst::CallInst(
    llvm::ArrayRef<Type> types,
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
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
  Set<-1>(cont);
}

// -----------------------------------------------------------------------------
CallInst::~CallInst()
{
}

// -----------------------------------------------------------------------------
Block *CallInst::getSuccessor(unsigned i)
{
  if (i == 0) return GetCont();
  llvm_unreachable("invalid successor index");
}

// -----------------------------------------------------------------------------
const Block *CallInst::GetCont() const
{
  return cast<Block>(Get<-1>()).Get();
}

// -----------------------------------------------------------------------------
Block *CallInst::GetCont()
{
  return cast<Block>(Get<-1>()).Get();
}

// -----------------------------------------------------------------------------
template<>
CallSite *cast_or_null<CallSite>(Value *value)
{
  if (auto *call = ::cast_or_null<CallInst>(value)) {
    return call;
  }
  if (auto *call = ::cast_or_null<TailCallInst>(value)) {
    return call;
  }
  if (auto *call = ::cast_or_null<InvokeInst>(value)) {
    return call;
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
TailCallInst::TailCallInst(
    llvm::ArrayRef<Type> types,
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
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
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
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
Block *TailCallInst::getSuccessor(unsigned i)
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
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
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
  Set<-2>(jcont);
  Set<-1>(jthrow);
}

// -----------------------------------------------------------------------------
InvokeInst::InvokeInst(
    llvm::ArrayRef<Type> types,
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
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
  Set<-2>(jcont);
  Set<-1>(jthrow);
}

// -----------------------------------------------------------------------------
Block *InvokeInst::getSuccessor(unsigned i)
{
  if (i == 0) return GetCont();
  if (i == 1) return GetThrow();
  llvm_unreachable("invalid successor");
}

// -----------------------------------------------------------------------------
unsigned InvokeInst::getNumSuccessors() const
{
  return 2;
}

// -----------------------------------------------------------------------------
const Block *InvokeInst::GetCont() const
{
  return cast<Block>(Get<-2>()).Get();
}

// -----------------------------------------------------------------------------
Block *InvokeInst::GetCont()
{
  return cast<Block>(Get<-2>()).Get();
}

// -----------------------------------------------------------------------------
const Block *InvokeInst::GetThrow() const
{
  return cast<Block>(Get<-1>()).Get();
}

// -----------------------------------------------------------------------------
Block *InvokeInst::GetThrow()
{
  return cast<Block>(Get<-1>()).Get();
}

// -----------------------------------------------------------------------------
Ref<Inst> GetCalledInst(Inst *inst)
{
  switch (inst->GetKind()) {
    case Inst::Kind::CALL: {
      return ::cast<CallInst>(inst)->GetCallee();
    }
    case Inst::Kind::INVOKE: {
      return ::cast<InvokeInst>(inst)->GetCallee();
    }
    case Inst::Kind::TCALL: {
      return ::cast<TailCallInst>(inst)->GetCallee();
    }
    default: {
      return {};
    }
  }
}

// -----------------------------------------------------------------------------
Func *GetCallee(Inst *inst)
{
  Ref<Inst> callee = nullptr;
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

  if (auto movInst = ::cast_or_null<MovInst>(callee)) {
    return ::cast_or_null<Func>(movInst->GetArg()).Get();
  } else {
    return nullptr;
  }
}
