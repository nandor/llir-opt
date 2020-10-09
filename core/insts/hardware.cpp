// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/inst.h"
#include "core/insts/hardware.h"



// -----------------------------------------------------------------------------
SetInst::SetInst(ConstantReg *reg, Inst *val, AnnotSet &&annot)
  : Inst(Kind::SET, 2, std::move(annot))
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
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
RdtscInst::RdtscInst(Type type, AnnotSet &&annot)
  : OperatorInst(Inst::Kind::RDTSC, type, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
FNStCwInst::FNStCwInst(Inst *addr, AnnotSet &&annot)
  : Inst(Kind::FNSTCW, 1, std::move(annot))
{
  Op<0>() = addr;
}

// -----------------------------------------------------------------------------
unsigned FNStCwInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type FNStCwInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
FLdCwInst::FLdCwInst(Inst *addr, AnnotSet &&annot)
  : Inst(Kind::FLDCW, 1, std::move(annot))
{
  Op<0>() = addr;
}

// -----------------------------------------------------------------------------
unsigned FLdCwInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type FLdCwInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
SyscallInst::SyscallInst(
    std::optional<Type> type,
    Inst *sysno,
    const std::vector<Inst *> &args,
    AnnotSet &&annot)
  : Inst(Kind::SYSCALL, args.size() + 1, std::move(annot))
  , type_(type)
{
  Op<0>() = sysno;
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    *(this->op_begin() + i + 1) = args[i];
  }
}

// -----------------------------------------------------------------------------
unsigned SyscallInst::GetNumRets() const
{
  return type_ ? 1 : 0;
}

// -----------------------------------------------------------------------------
Type SyscallInst::GetType(unsigned i) const
{
  if (i == 0 && type_) return *type_;
  llvm_unreachable("invalid argument index");
}

// -----------------------------------------------------------------------------
CloneInst::CloneInst(
    Type type,
    Inst *callee,
    Inst *stack,
    Inst *flag,
    Inst *arg,
    Inst *tpid,
    Inst *tls,
    Inst *ctid,
    AnnotSet &&annot)
  : ControlInst(Kind::CLONE, 7, std::move(annot))
  , type_(type)
{
  Op<0>() = callee;
  Op<1>() = stack;
  Op<2>() = flag;
  Op<3>() = arg;
  Op<4>() = tpid;
  Op<5>() = tls;
  Op<6>() = ctid;
}

// -----------------------------------------------------------------------------
unsigned CloneInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type CloneInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid type index");
}
