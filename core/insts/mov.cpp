// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts.h"



// -----------------------------------------------------------------------------
MovInst::MovInst(Type type, Ref<Value> op, AnnotSet &&annot)
  : OperatorInst(Kind::MOV, 1, type, std::move(annot))
{
  Set<0>(op);
}

// -----------------------------------------------------------------------------
MovInst::MovInst(Type type, Ref<Value> op, const AnnotSet &annot)
  : OperatorInst(Kind::MOV, 1, type, annot)
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
