// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts.h"


// -----------------------------------------------------------------------------
ArgInst::ArgInst(Type type, unsigned index, AnnotSet &&annot)
  : ConstInst(Kind::ARG, type, 1, std::move(annot))
{
  Set<0>(new ConstantInt(index));
}

// -----------------------------------------------------------------------------
ArgInst::ArgInst(Type type, Ref<ConstantInt> index, AnnotSet &&annot)
  : ConstInst(Kind::ARG, type, 1, std::move(annot))
{
  Set<0>(index);
}

// -----------------------------------------------------------------------------
ArgInst::ArgInst(Type type, Ref<ConstantInt> index, const AnnotSet &annot)
  : ConstInst(Kind::ARG, type, 1, annot)
{
  Set<0>(index);
}

// -----------------------------------------------------------------------------
unsigned ArgInst::GetIdx() const
{
  return cast<ConstantInt>(Get<0>())->GetInt();
}

// -----------------------------------------------------------------------------
FrameInst::FrameInst(
    Type type,
    unsigned object,
    unsigned index,
    AnnotSet &&annot)
  : ConstInst(Kind::FRAME, type, 2, std::move(annot))
{
  Set<0>(new ConstantInt(object));
  Set<1>(new ConstantInt(index));
}

// -----------------------------------------------------------------------------
FrameInst::FrameInst(
    Type type,
    Ref<ConstantInt> object,
    Ref<ConstantInt> index,
    AnnotSet &&annot)
  : ConstInst(Kind::FRAME, type, 2, std::move(annot))
{
  Set<0>(object);
  Set<1>(index);
}

// -----------------------------------------------------------------------------
FrameInst::FrameInst(
    Type type,
    Ref<ConstantInt> object,
    Ref<ConstantInt> index,
    const AnnotSet &annot)
  : ConstInst(Kind::FRAME, type, 2, annot)
{
  Set<0>(object);
  Set<1>(index);
}

// -----------------------------------------------------------------------------
unsigned FrameInst::GetObject() const
{
  return cast<ConstantInt>(Get<0>())->GetInt();
}

// -----------------------------------------------------------------------------
unsigned FrameInst::GetOffset() const
{
  return cast<ConstantInt>(Get<1>())->GetInt();
}

// -----------------------------------------------------------------------------
UndefInst::UndefInst(Type type, AnnotSet &&annot)
  : ConstInst(Kind::UNDEF, type, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
UndefInst::UndefInst(Type type, const AnnotSet &annot)
  : ConstInst(Kind::UNDEF, type, 0, annot)
{
}

// -----------------------------------------------------------------------------
MovInst::MovInst(Type type, Ref<Value> op, AnnotSet &&annot)
  : OperatorInst(Kind::MOV, type, 1, std::move(annot))
{
  Set<0>(op);
}

// -----------------------------------------------------------------------------
MovInst::MovInst(Type type, Ref<Value> op, const AnnotSet &annot)
  : OperatorInst(Kind::MOV, type, 1, annot)
{
  Set<0>(op);
}

// -----------------------------------------------------------------------------
MovInst::~MovInst()
{
  if (auto *v = GetArg().Get(); v && !(reinterpret_cast<uintptr_t>(v) & 1)) {
    Set<0>(nullptr);
    switch (v->GetKind()) {
      case Value::Kind::INST:
      case Value::Kind::GLOBAL: {
        return;
      }
      case Value::Kind::EXPR:
      case Value::Kind::CONST: {
        if (v->use_empty()) {
          delete v;
        }
        return;
      }
    }
    llvm_unreachable("invalid argument kind");
  }
}

// -----------------------------------------------------------------------------
ConstRef<Value> MovInst::GetArg() const
{
  return Get<0>();
}

// -----------------------------------------------------------------------------
Ref<Value> MovInst::GetArg()
{
  return Get<0>();
}
