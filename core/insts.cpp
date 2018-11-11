// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/context.h"
#include "core/insts.h"
#include "core/symbol.h"



// -----------------------------------------------------------------------------
unsigned CallInst::GetNumOps() const
{
  return 1 + args_.size();
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
const Operand &CallInst::GetOp(unsigned i) const
{
  if (i == 0) return callee_;
  if (i <= args_.size()) return args_[i - 1];
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void CallInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { callee_ = op; return; }
  if (i <= args_.size()) { args_[i - 1] = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned TailCallInst::GetNumOps() const
{
  return 1 + args_.size();
}

// -----------------------------------------------------------------------------
const Operand &TailCallInst::GetOp(unsigned i) const
{
  if (i == 0) return callee_;
  if (i <= args_.size()) return args_[i - 1];
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void TailCallInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { callee_ = op; return; }
  if (i <= args_.size()) { args_[i - 1] = op; return; }
  throw InvalidOperandException();
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
unsigned InvokeInst::GetNumOps() const
{
  return 3 + args_.size();
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
const Operand &InvokeInst::GetOp(unsigned i) const
{
  if (i == 0) return callee_;
  if (i <= args_.size()) return args_[i - 1];
  if (i == args_.size() + 1) return jcont_;
  if (i == args_.size() + 2) return jthrow_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void InvokeInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { callee_ = op; return; }
  if (i <= args_.size()) { args_[i - 1] = op; return; }
  if (i == args_.size() + 1) { jcont_ = op; return; }
  if (i == args_.size() + 2) { jthrow_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
Block *InvokeInst::getSuccessor(unsigned i) const
{
  if (i == 0) { return jcont_.GetBlock(); }
  if (i == 1) { return jthrow_.GetBlock(); }
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned InvokeInst::getNumSuccessors() const
{
  return 2;
}

// -----------------------------------------------------------------------------
unsigned ReturnInst::GetNumOps() const
{
  return op_ ? 1 : 0;
}

// -----------------------------------------------------------------------------
const Operand &ReturnInst::GetOp(unsigned i) const
{
  if (i == 0 && op_) return *op_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void ReturnInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0 && op_) { op_ = op; return; }
  throw InvalidOperandException();
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
unsigned JumpCondInst::GetNumOps() const
{
  return 3;
}

// -----------------------------------------------------------------------------
const Operand &JumpCondInst::GetOp(unsigned i) const
{
  if (i == 0) return cond_;
  if (i == 1) return bt_;
  if (i == 2) return bf_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void JumpCondInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { cond_ = op; return; }
  if (i == 1) { bt_ = op; return; }
  if (i == 2) { bf_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
Block *JumpCondInst::getSuccessor(unsigned i) const
{
  if (i == 0) return bt_.GetBlock();
  if (i == 1) return bf_.GetBlock();
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned JumpCondInst::getNumSuccessors() const
{
  return 2;
}

// -----------------------------------------------------------------------------
unsigned JumpIndirectInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &JumpIndirectInst::GetOp(unsigned i) const
{
  if (i == 0) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void JumpIndirectInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { target_ = op; return; }
  throw InvalidOperandException();
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
unsigned JumpInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &JumpInst::GetOp(unsigned i) const
{
  if (i == 0) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void JumpInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { target_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
Block *JumpInst::getSuccessor(unsigned i) const
{
  if (i == 0) return target_.GetBlock();
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned JumpInst::getNumSuccessors() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned SelectInst::GetNumOps() const
{
  return 3;
}

// -----------------------------------------------------------------------------
const Operand &SelectInst::GetOp(unsigned i) const
{
  if (i == 0) return cond_;
  if (i == 1) return vt_;
  if (i == 2) return vf_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void SelectInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { cond_ = op; return; }
  if (i == 1) { vt_ = op; return; }
  if (i == 2) { vf_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned SwitchInst::GetNumOps() const
{
  return 1 + branches_.size();
}

// -----------------------------------------------------------------------------
const Operand &SwitchInst::GetOp(unsigned i) const
{
  if (i == 0) return index_;
  if (i <= branches_.size()) return branches_[i - 1];
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void SwitchInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { index_ = op; return; }
  if (i <= branches_.size()) { branches_[i - 1] = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
Block *SwitchInst::getSuccessor(unsigned i) const
{
  if (i < branches_.size()) return branches_[i].GetBlock();
  throw InvalidSuccessorException();
}

// -----------------------------------------------------------------------------
unsigned SwitchInst::getNumSuccessors() const
{
  return branches_.size();
}

// -----------------------------------------------------------------------------
unsigned TrapInst::GetNumOps() const
{
  return 0;
}

// -----------------------------------------------------------------------------
const Operand &TrapInst::GetOp(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void TrapInst::SetOp(unsigned i, const Operand &op)
{
  throw InvalidOperandException();
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
unsigned ExchangeInst::GetNumOps() const
{
  return 2;
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
const Operand &ExchangeInst::GetOp(unsigned i) const
{
  if (i == 0) return addr_;
  if (i == 1) return val_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void ExchangeInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { addr_ = op; return; }
  if (i == 1) { val_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned SetInst::GetNumOps() const
{
  return 2;
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
const Operand &SetInst::GetOp(unsigned i) const
{
  if (i == 0) return reg_;
  if (i == 1) return val_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void SetInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { reg_ = op; return; }
  if (i == 1) { val_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned ImmInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned ImmInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type ImmInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
const Operand &ImmInst::GetOp(unsigned i) const
{
  if (i == 0) return imm_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void ImmInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { imm_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
int64_t ImmInst::GetInt() const
{
  return imm_.GetInt();
}

// -----------------------------------------------------------------------------
double ImmInst::GetFloat() const
{
  return imm_.GetFloat();
}

// -----------------------------------------------------------------------------
unsigned ArgInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned ArgInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type ArgInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
const Operand &ArgInst::GetOp(unsigned i) const
{
  if (i == 0) return index_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void ArgInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { index_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned AddrInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned AddrInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type AddrInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
const Operand &AddrInst::GetOp(unsigned i) const
{
  if (i == 0) return addr_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void AddrInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { addr_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned LoadInst::GetNumOps() const
{
  return 1;
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
const Operand &LoadInst::GetOp(unsigned i) const
{
  if (i == 0) return addr_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void LoadInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { addr_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
std::optional<size_t> LoadInst::GetSize() const
{
  return size_;
}

// -----------------------------------------------------------------------------
unsigned PushInst::GetNumOps() const
{
  return 1;
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
const Operand &PushInst::GetOp(unsigned i) const
{
  if (i == 0) return val_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void PushInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { val_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned PopInst::GetNumOps() const
{
  return 0;
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
const Operand &PopInst::GetOp(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void PopInst::SetOp(unsigned i, const Operand &op)
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned StoreInst::GetNumOps() const
{
  return 2;
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
const Operand &StoreInst::GetOp(unsigned i) const
{
  if (i == 0) return addr_;
  if (i == 1) return val_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void StoreInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { addr_ = op; return; }
  if (i == 1) { val_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
std::optional<size_t> StoreInst::GetSize() const
{
  return size_;
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumOps() const
{
  return ops_.size() * 2;
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
const Operand &PhiInst::GetOp(unsigned i) const
{
  unsigned idx = i >> 1;
  if (idx < ops_.size() && (i & 1) == 0) return ops_[idx].first;
  if (idx < ops_.size() && (i & 1) == 1) return ops_[idx].second;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void PhiInst::SetOp(unsigned i, const Operand &op)
{
  unsigned idx = i >> 1;
  if (idx < ops_.size() && (i & 1) == 0) { ops_[idx].first = op; return; }
  if (idx < ops_.size() && (i & 1) == 1) { ops_[idx].second = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void PhiInst::Add(Block *block, const Operand &value)
{
  for (auto &op : ops_) {
    if (op.first.GetBlock() == block) {
      op.second = value;
      return;
    }
  }
  ops_.emplace_back(block, value);
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumIncoming() const
{
  return ops_.size();
}

// -----------------------------------------------------------------------------
Block *PhiInst::GetBlock(unsigned i) const
{
  return ops_[i].first.GetBlock();
}

// -----------------------------------------------------------------------------
const Operand &PhiInst::GetValue(unsigned i) const
{
  return ops_[i].second;
}

// -----------------------------------------------------------------------------
const Operand &PhiInst::GetValue(Block *block) const
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return GetValue(i);
    }
  }
  throw InvalidPredecessorException();
}
