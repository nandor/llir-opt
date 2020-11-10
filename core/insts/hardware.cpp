// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/inst.h"
#include "core/insts/hardware.h"



// -----------------------------------------------------------------------------
SetInst::SetInst(Ref<ConstantReg> reg, Ref<Inst> val, AnnotSet &&annot)
  : Inst(Kind::SET, 2, std::move(annot))
{
  Set<0>(reg);
  Set<1>(val);
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
ConstRef<ConstantReg> SetInst::GetReg() const
{
  return cast<ConstantReg>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<ConstantReg> SetInst::GetReg()
{
  return cast<ConstantReg>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> SetInst::GetValue() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> SetInst::GetValue()
{
  return cast<Inst>(Get<1>());
}


// -----------------------------------------------------------------------------
SyscallInst::SyscallInst(
    std::optional<Type> type,
    Ref<Inst> sysno,
    const std::vector<Ref<Inst> > &args,
    AnnotSet &&annot)
  : Inst(Kind::SYSCALL, args.size() + 1, std::move(annot))
  , type_(type)
{
  Set<0>(sysno);
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    Set(i + 1, args[i]);
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
ConstRef<Inst> SyscallInst::GetSyscall() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> SyscallInst::GetSyscall()
{
  return cast<Inst>(Get<0>());
}


// -----------------------------------------------------------------------------
CloneInst::CloneInst(
    Type type,
    Ref<Inst> callee,
    Ref<Inst> stack,
    Ref<Inst> flag,
    Ref<Inst> arg,
    Ref<Inst> tpid,
    Ref<Inst> tls,
    Ref<Inst> ctid,
    AnnotSet &&annot)
  : ControlInst(Kind::CLONE, 7, std::move(annot))
  , type_(type)
{
  Set<0>(callee);
  Set<1>(stack);
  Set<2>(flag);
  Set<3>(arg);
  Set<4>(tpid);
  Set<5>(tls);
  Set<6>(ctid);
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

// -----------------------------------------------------------------------------
ConstRef<Inst> CloneInst::GetCallee() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> CloneInst::GetCallee()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> CloneInst::GetStack() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> CloneInst::GetStack()
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> CloneInst::GetFlags() const
{
  return cast<Inst>(Get<2>());
}

// -----------------------------------------------------------------------------
Ref<Inst> CloneInst::GetFlags()
{
  return cast<Inst>(Get<2>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> CloneInst::GetArg() const
{
  return cast<Inst>(Get<3>());
}

// -----------------------------------------------------------------------------
Ref<Inst> CloneInst::GetArg()
{
  return cast<Inst>(Get<3>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> CloneInst::GetPTID() const
{
  return cast<Inst>(Get<4>());
}

// -----------------------------------------------------------------------------
Ref<Inst> CloneInst::GetPTID()
{
  return cast<Inst>(Get<4>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> CloneInst::GetTLS() const
{
  return cast<Inst>(Get<5>());
}

// -----------------------------------------------------------------------------
Ref<Inst> CloneInst::GetTLS()
{
  return cast<Inst>(Get<5>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> CloneInst::GetCTID() const
{
  return cast<Inst>(Get<6>());
}

// -----------------------------------------------------------------------------
Ref<Inst> CloneInst::GetCTID()
{
  return cast<Inst>(Get<6>());
}
