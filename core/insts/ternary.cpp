// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/insts/ternary.h"



// -----------------------------------------------------------------------------
SelectInst::SelectInst(
    Type type,
    Ref<Inst> cond,
    Ref<Inst> vt,
    Ref<Inst> vf,
    AnnotSet &&annot)
  : OperatorInst(Kind::SELECT, type, 3, std::move(annot))
{
  Set<0>(cond);
  Set<1>(vt);
  Set<2>(vf);
}

// -----------------------------------------------------------------------------
SelectInst::SelectInst(
    Type type,
    Ref<Inst> cond,
    Ref<Inst> vt,
    Ref<Inst> vf,
    const AnnotSet &annot)
  : OperatorInst(Kind::SELECT, type, 3, annot)
{
  Set<0>(cond);
  Set<1>(vt);
  Set<2>(vf);
}


// -----------------------------------------------------------------------------
ConstRef<Inst> SelectInst::GetCond() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> SelectInst::GetCond()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> SelectInst::GetTrue() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> SelectInst::GetTrue()
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> SelectInst::GetFalse() const
{
  return cast<Inst>(Get<2>());
}

// -----------------------------------------------------------------------------
Ref<Inst> SelectInst::GetFalse()
{
  return cast<Inst>(Get<2>());
}
