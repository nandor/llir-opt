// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/insts.h"



// -----------------------------------------------------------------------------
CallSite::CallSite(
    Inst::Kind kind,
    unsigned numOps,
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
    llvm::ArrayRef<TypeFlag> flags,
    std::optional<unsigned> numFixed,
    CallingConv conv,
    llvm::ArrayRef<Type> types,
    AnnotSet &&annot)
  : TerminatorInst(kind, numOps, std::move(annot))
  , numArgs_(args.size())
  , numFixed_(numFixed)
  , conv_(conv)
  , types_(types)
  , flags_(flags)
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
    llvm::ArrayRef<TypeFlag> flags,
    std::optional<unsigned> numFixed,
    CallingConv conv,
    llvm::ArrayRef<Type> types,
    const AnnotSet &annot)
  : TerminatorInst(kind, numOps, annot)
  , numArgs_(args.size())
  , numFixed_(numFixed)
  , conv_(conv)
  , types_(types)
  , flags_(flags)
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
Func *CallSite::GetDirectCallee()
{
  Ref<Inst> callee = GetCallee();
  while (auto mov = ::cast_or_null<MovInst>(callee)) {
    auto movArg = mov->GetArg();
    if (auto func = ::cast_or_null<Func>(movArg)) {
      return func.Get();
    }
    if (auto inst = ::cast_or_null<Inst>(movArg)) {
      callee = inst;
      continue;
    }
    break;
  }

  return nullptr;
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
    llvm::ArrayRef<TypeFlag> flags,
    Block *cont,
    std::optional<unsigned> numFixed,
    CallingConv conv,
    AnnotSet &&annot)
  : CallSite(
        Inst::Kind::CALL,
        args.size() + 2,
        callee,
        args,
        flags,
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
    llvm::ArrayRef<TypeFlag> flags,
    Block *cont,
    std::optional<unsigned> numFixed,
    CallingConv conv,
    const AnnotSet &annot)
  : CallSite(
        Inst::Kind::CALL,
        args.size() + 2,
        callee,
        args,
        flags,
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
const Block *CallInst::getSuccessor(unsigned i) const
{
  if (i == 0) return GetCont();
  llvm_unreachable("invalid successor index");
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
TailCallInst::TailCallInst(
    llvm::ArrayRef<Type> types,
    Ref<Inst> callee,
    llvm::ArrayRef<Ref<Inst>> args,
    llvm::ArrayRef<TypeFlag> flags,
    std::optional<unsigned> numFixed,
    CallingConv conv,
    AnnotSet &&annot)
  : CallSite(
        Kind::TAIL_CALL,
        args.size() + 1,
        callee,
        args,
        flags,
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
    llvm::ArrayRef<TypeFlag> flags,
    std::optional<unsigned> numFixed,
    CallingConv conv,
    const AnnotSet &annot)
  : CallSite(
        Kind::TAIL_CALL,
        args.size() + 1,
        callee,
        args,
        flags,
        numFixed,
        conv,
        types,
        annot
    )
{
}

// -----------------------------------------------------------------------------
const Block *TailCallInst::getSuccessor(unsigned i) const
{
  llvm_unreachable("invalid successor");
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
    llvm::ArrayRef<TypeFlag> flags,
    Block *jcont,
    Block *jthrow,
    std::optional<unsigned> numFixed,
    CallingConv conv,
    AnnotSet &&annot)
  : CallSite(
        Kind::INVOKE,
        args.size() + 3,
        callee,
        args,
        flags,
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
    llvm::ArrayRef<TypeFlag> flags,
    Block *jcont,
    Block *jthrow,
    std::optional<unsigned> numFixed,
    CallingConv conv,
    const AnnotSet &annot)
  : CallSite(
        Kind::INVOKE,
        args.size() + 3,
        callee,
        args,
        flags,
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
const Block *InvokeInst::getSuccessor(unsigned i) const
{
  if (i == 0) return GetCont();
  if (i == 1) return GetThrow();
  llvm_unreachable("invalid successor");
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
