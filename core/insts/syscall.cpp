// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/inst.h"
#include "core/insts/syscall.h"



// -----------------------------------------------------------------------------
SyscallInst::SyscallInst(
    llvm::ArrayRef<Type> types,
    Ref<Inst> sysno,
    const std::vector<Ref<Inst> > &args,
    AnnotSet &&annot)
  : Inst(Kind::SYSCALL, args.size() + 1, std::move(annot))
  , types_(types)
{
  Set<0>(sysno);
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    Set(i + 1, args[i]);
  }
}

// -----------------------------------------------------------------------------
SyscallInst::SyscallInst(
    std::optional<Type> type,
    Ref<Inst> sysno,
    const std::vector<Ref<Inst> > &args,
    AnnotSet &&annot)
  : Inst(Kind::SYSCALL, args.size() + 1, std::move(annot))
  , types_(type ? std::vector<Type>{*type} : std::vector<Type>{})
{
  Set<0>(sysno);
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    Set(i + 1, args[i]);
  }
}

// -----------------------------------------------------------------------------
unsigned SyscallInst::GetNumRets() const
{
  return types_.size();
}

// -----------------------------------------------------------------------------
Type SyscallInst::GetType(unsigned i) const
{
  return types_[i];
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
ConstRef<Inst> SyscallInst::arg(unsigned i) const
{
  return cast<Inst>(static_cast<ConstRef<Value>>(Get(i + 1)));
}

// -----------------------------------------------------------------------------
Ref<Inst> SyscallInst::arg(unsigned i)
{
  return cast<Inst>(static_cast<Ref<Value>>(Get(i + 1)));
}
