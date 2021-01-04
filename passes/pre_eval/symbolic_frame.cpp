// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>

#include "core/inst.h"
#include "passes/pre_eval/symbolic_frame.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(
    Func &func,
    unsigned index,
    llvm::ArrayRef<SymbolicValue> args)
  : func_(&func)
  , index_(index)
  , valid_(true)
  , args_(args)
{
  Initialise(func.objects());
}

// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(
    unsigned index,
    llvm::ArrayRef<Func::StackObject> objects)
  : func_(nullptr)
  , index_(index)
  , valid_(true)
{
  Initialise(objects);
}

// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(const SymbolicFrame &that)
  : func_(that.func_)
  , index_(that.index_)
  , valid_(that.valid_)
  , args_(that.args_)
  , values_(that.values_)
{
  for (auto &[id, object] : that.objects_) {
    objects_.emplace(id, std::make_unique<SymbolicFrameObject>(*this, *object));
  }
}

// -----------------------------------------------------------------------------
bool SymbolicFrame::Set(Ref<Inst> i, const SymbolicValue &value)
{
  auto it = values_.emplace(i, value);
  if (it.second) {
    return true;
  }
  auto &oldValue = it.first->second;
  if (oldValue == value) {
    return false;
  }
  oldValue = value;
  return true;
}

// -----------------------------------------------------------------------------
const SymbolicValue &SymbolicFrame::Find(ConstRef<Inst> inst)
{
  auto it = values_.find(inst);
  assert(it != values_.end() && "value not computed");
  return it->second;
}

// -----------------------------------------------------------------------------
const SymbolicValue *SymbolicFrame::FindOpt(ConstRef<Inst> inst)
{
  auto it = values_.find(inst);
  if (it == values_.end()) {
    return nullptr;
  } else {
    return &it->second;
  }
}

// -----------------------------------------------------------------------------
void SymbolicFrame::Initialise(llvm::ArrayRef<Func::StackObject> objects)
{
  for (auto &object : objects) {
    objects_.emplace(
        object.Index,
        std::make_unique<SymbolicFrameObject>(
            *this,
            object.Index,
            object.Size,
            object.Alignment
        )
    );
  }
}

// -----------------------------------------------------------------------------
void SymbolicFrame::LUB(const SymbolicFrame &that)
{
  for (auto &[id, object] : that.objects_) {
    if (auto it = objects_.find(id); it != objects_.end()) {
      it->second->LUB(*object);
    } else {
      llvm_unreachable("not implemented");
    }
  }
  /*
  for (auto &[id, value] : that.values_) {
    if (auto it = values_.find(id); it != values_.end()) {
      llvm_unreachable("not implemented");
    } else {
      llvm_unreachable("not implemented");
    }
  }
  */
}
