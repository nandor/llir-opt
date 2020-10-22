// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts/memory.h"



// -----------------------------------------------------------------------------
LoadInst::LoadInst(Type type, Ref<Inst> addr, AnnotSet &&annot)
  : MemoryInst(Kind::LD, 1, std::move(annot))
  , type_(type)
{
  Set<0>(addr);
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
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
ConstRef<Inst> LoadInst::GetAddr() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> LoadInst::GetAddr()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
StoreInst::StoreInst(
    Ref<Inst> addr,
    Ref<Inst> val,
    AnnotSet &&annot)
  : MemoryInst(Kind::ST, 2, std::move(annot))
{
  Set<0>(addr);
  Set<1>(val);
}

// -----------------------------------------------------------------------------
unsigned StoreInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type StoreInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
ConstRef<Inst> StoreInst::GetAddr() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> StoreInst::GetAddr()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> StoreInst::GetVal() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> StoreInst::GetVal()
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
VAStartInst::VAStartInst(Ref<Inst> vaList, AnnotSet &&annot)
  : Inst(Kind::VASTART, 1, std::move(annot))
{
  Set<0>(vaList);
}

// -----------------------------------------------------------------------------
unsigned VAStartInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type VAStartInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
ConstRef<Inst> VAStartInst::GetVAList() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> VAStartInst::GetVAList()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
AllocaInst::AllocaInst(
    Type type,
    Ref<Inst> size,
    ConstantInt *align,
    AnnotSet &&annot)
  : OperatorInst(Kind::ALLOCA, type, 2, std::move(annot))
{
  Set<0>(size);
  Set<1>(align);
}

// -----------------------------------------------------------------------------
ConstRef<Inst> AllocaInst::GetCount() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> AllocaInst::GetCount()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
int AllocaInst::GetAlign() const
{
  return cast<ConstantInt>(Get<1>())->GetInt();
}
