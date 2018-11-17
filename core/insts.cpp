// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/context.h"
#include "core/insts.h"
#include "core/symbol.h"



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

// -----------------------------------------------------------------------------
ReturnInst::ReturnInst(Block *block)
  : TerminatorInst(Kind::RET, block, 0)
{
}

// -----------------------------------------------------------------------------
ReturnInst::ReturnInst(Block *block, Type t, Inst *op)
  : TerminatorInst(Kind::RET, block, 1)
{
  Op<0>() = op;
}

// -----------------------------------------------------------------------------
Block *ReturnInst::getSuccessor(unsigned i) const
{
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned ReturnInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Inst *ReturnInst::GetValue() const
{
  return numOps_ > 0 ? static_cast<Inst *>(Op<0>().get()) : nullptr;
}

// -----------------------------------------------------------------------------
JumpCondInst::JumpCondInst(Block *block, Value *cond, Block *bt, Block *bf)
  : TerminatorInst(Kind::JCC, block, 3)
{
  Op<0>() = cond;
  Op<1>() = bt;
  Op<2>() = bf;
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::getSuccessor(unsigned i) const
{
  if (i == 0) return GetTrueTarget();
  if (i == 1) return GetFalseTarget();
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned JumpCondInst::getNumSuccessors() const
{
  return 2;
}

// -----------------------------------------------------------------------------
Inst *JumpCondInst::GetCond() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::GetTrueTarget() const
{
  return static_cast<Block *>(Op<1>().get());
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::GetFalseTarget() const
{
  return static_cast<Block *>(Op<2>().get());
}

// -----------------------------------------------------------------------------
JumpIndirectInst::JumpIndirectInst(Block *block, Inst *target)
  : TerminatorInst(Kind::JI, block, 1)
{
  Op<0>() = target;
}

// -----------------------------------------------------------------------------
Block *JumpIndirectInst::getSuccessor(unsigned i) const
{
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned JumpIndirectInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
JumpInst::JumpInst(Block *block, Value *target)
  : TerminatorInst(Kind::JMP, block, 1)
{
  Op<0>() = target;
}

// -----------------------------------------------------------------------------
Block *JumpInst::getSuccessor(unsigned i) const
{
  if (i == 0) return static_cast<Block *>(Op<0>().get());
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned JumpInst::getNumSuccessors() const
{
  return 1;
}

// -----------------------------------------------------------------------------
SelectInst::SelectInst(Block *block, Type type, Inst *cond, Inst *vt, Inst *vf)
  : OperatorInst(Kind::SELECT, block, type, 3)
{
  Op<0>() = cond;
  Op<1>() = vt;
  Op<2>() = vf;
}

// -----------------------------------------------------------------------------
SwitchInst::SwitchInst(
    Block *block,
    Inst *index,
    const std::vector<Value *> &branches)
  : TerminatorInst(Kind::SWITCH, block, branches.size() + 1)
{
  Op<0>() = index;
  for (unsigned i = 0, n = branches.size(); i < n; ++i) {
    *(op_begin() + i + 1) = branches[i];
  }
}

// -----------------------------------------------------------------------------
Block *SwitchInst::getSuccessor(unsigned i) const
{
  if (i + 1 < numOps_) return static_cast<Block *>((op_begin() + i + 1)->get());
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned SwitchInst::getNumSuccessors() const
{
  return numOps_ - 1;
}

// -----------------------------------------------------------------------------
Block *TrapInst::getSuccessor(unsigned i) const
{
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned TrapInst::getNumSuccessors() const
{
  return 0;
}

// -----------------------------------------------------------------------------
ExchangeInst::ExchangeInst(Block *block, Type type, Inst *addr, Inst *val)
  : MemoryInst(Kind::XCHG, block, 2)
  , type_(type)
{
  Op<0>() = addr;
  Op<1>() = val;
}

// -----------------------------------------------------------------------------
unsigned ExchangeInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type ExchangeInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
SetInst::SetInst(Block *block, Value *reg, Inst *val)
  : Inst(Kind::SET, block, 2)
{
  Op<0>() = reg;
  Op<1>() = val;
}

// -----------------------------------------------------------------------------
unsigned SetInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type SetInst::GetType(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
ImmInst::ImmInst(Block *block, Type type, Constant *imm)
  : ConstInst(Kind::IMM, block, type, 1)
{
  Op<0>() = imm;
}

// -----------------------------------------------------------------------------
union ImmValue {
  float f32v;
  double f64v;
  int8_t i8v;
  int16_t i16v;
  int32_t i32v;
  int64_t i64v;
};

// -----------------------------------------------------------------------------
ImmValue GetValue(Constant *cst)
{
  ImmValue val;
  switch (cst->GetKind()) {
    case Constant::Kind::INT: {
      val.i64v = static_cast<ConstantInt *>(cst)->GetValue();
      break;
    }
    case Constant::Kind::FLOAT: {
      val.f64v = static_cast<ConstantFloat *>(cst)->GetValue();
      break;
    }
    case Constant::Kind::REG: {
      assert(!"invalid constant");
      break;
    }
    case Constant::Kind::UNDEF: {
      assert(!"invalid constant");
      break;
    }
  }
  return val;
}

// -----------------------------------------------------------------------------
int64_t ImmInst::GetI8() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).i8v;
}

// -----------------------------------------------------------------------------
int64_t ImmInst::GetI16() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).i16v;
}

// -----------------------------------------------------------------------------
int64_t ImmInst::GetI32() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).i32v;
}

// -----------------------------------------------------------------------------
int64_t ImmInst::GetI64() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).i64v;
}

// -----------------------------------------------------------------------------
double ImmInst::GetF32() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).f32v;
}

// -----------------------------------------------------------------------------
double ImmInst::GetF64() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).f64v;
}

// -----------------------------------------------------------------------------
ArgInst::ArgInst(Block *block, Type type, ConstantInt *index)
  : ConstInst(Kind::ARG, block, type, 1)
{
  Op<0>() = index;
}

// -----------------------------------------------------------------------------
unsigned ArgInst::GetIdx() const
{
  return static_cast<ConstantInt *>(Op<0>().get())->GetValue();
}

// -----------------------------------------------------------------------------
AddrInst::AddrInst(Block *block, Type type, Value *addr)
  : ConstInst(Kind::ADDR, block, type, 1)
{
  Op<0>() = addr;
}

// -----------------------------------------------------------------------------
Value *AddrInst::GetAddr() const
{
  return Op<0>();
}

// -----------------------------------------------------------------------------
LoadInst::LoadInst(Block *block, size_t size, Type type, Value *addr)
  : MemoryInst(Kind::LD, block, 1)
  , size_(size)
  , type_(type)
{
  Op<0>() = addr;
}

// -----------------------------------------------------------------------------
unsigned LoadInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type LoadInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
std::optional<size_t> LoadInst::GetSize() const
{
  return size_;
}

// -----------------------------------------------------------------------------
const Inst *LoadInst::GetAddr() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
PushInst::PushInst(Block *block, Type type, Inst *val)
  : StackInst(Kind::PUSH, block, 1)
{
  Op<0>() = val;
}

// -----------------------------------------------------------------------------
unsigned PushInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type PushInst::GetType(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned PopInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type PopInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
StoreInst::StoreInst(
    Block *block,
    size_t size,
    Inst *addr,
    Inst *val)
  : MemoryInst(Kind::ST, block, 2)
  , size_(size)
{
  Op<0>() = addr;
  Op<1>() = val;
}

// -----------------------------------------------------------------------------
unsigned StoreInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type StoreInst::GetType(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
std::optional<size_t> StoreInst::GetSize() const
{
  return size_;
}

// -----------------------------------------------------------------------------
const Inst *StoreInst::GetAddr() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
const Inst *StoreInst::GetVal() const
{
  return static_cast<Inst *>(Op<1>().get());
}

// -----------------------------------------------------------------------------
PhiInst::PhiInst(Block *block, Type type)
  : Inst(Kind::PHI, block, 0)
  , type_(type)
{
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type PhiInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void PhiInst::Add(Block *block, Value *value)
{
  for (unsigned i = 0, n = GetNumIncoming(); i < n; ++i) {
    if (GetBlock(i) == block) {
      *(op_begin() + i * 2 + 1) = value;
      return;
    }
  }
  growUses(numOps_ + 2);
  Op<-2>() = block;
  Op<-1>() = value;
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumIncoming() const
{
  assert((numOps_ & 1) == 0 && "invalid node count");
  return numOps_ >> 1;
}

// -----------------------------------------------------------------------------
Block *PhiInst::GetBlock(unsigned i) const
{
  const Use *use = op_begin() + i * 2 + 0;
  return static_cast<Block *>((op_begin() + i * 2 + 0)->get());
}

// -----------------------------------------------------------------------------
Value *PhiInst::GetValue(unsigned i) const
{
  return static_cast<Block *>((op_begin() + i * 2 + 1)->get());
}

// -----------------------------------------------------------------------------
bool PhiInst::HasValue(const Block *block) const
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
Value *PhiInst::GetValue(const Block *block) const
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return GetValue(i);
    }
  }
  throw InvalidPredecessorException();
}
