// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/inst.h"
#include "core/insts/hardware_ppc.h"



// -----------------------------------------------------------------------------
PPC_LLInst::PPC_LLInst(Type type, Ref<Inst> addr, AnnotSet &&annot)
  : MemoryInst(Kind::PPC_LL, 1, std::move(annot))
  , type_(type)
{
  Set<0>(addr);
}

// -----------------------------------------------------------------------------
Type PPC_LLInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid return value");
}

// -----------------------------------------------------------------------------
ConstRef<Inst> PPC_LLInst::GetAddr() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> PPC_LLInst::GetAddr()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
PPC_SCInst::PPC_SCInst(
    Type type,
    Ref<Inst> addr,
    Ref<Inst> val,
    AnnotSet &&annot)
  : MemoryInst(Kind::PPC_SC, 2, std::move(annot))
  , type_(type)
{
  Set<0>(addr);
  Set<1>(val);
}

// -----------------------------------------------------------------------------
Type PPC_SCInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid return value");
}

// -----------------------------------------------------------------------------
ConstRef<Inst> PPC_SCInst::GetAddr() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> PPC_SCInst::GetAddr()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> PPC_SCInst::GetValue() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> PPC_SCInst::GetValue()
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
PPC_SyncInst::PPC_SyncInst(AnnotSet &&annot)
  : Inst(Kind::PPC_SYNC, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
Type PPC_SyncInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid return value");
}

// -----------------------------------------------------------------------------
PPC_ISyncInst::PPC_ISyncInst(AnnotSet &&annot)
  : Inst(Kind::PPC_ISYNC, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
Type PPC_ISyncInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid return value");
}


