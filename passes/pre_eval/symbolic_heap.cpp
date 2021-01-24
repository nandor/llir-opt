// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/ErrorHandling.h>

#include "passes/pre_eval/symbolic_heap.h"



// -----------------------------------------------------------------------------
ID<SymbolicObject> SymbolicHeap::Data(Object *object)
{
  auto it = objects_.emplace(object, next_);
  if (it.second) {
    next_++;
    origins_.emplace_back(object);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
ID<SymbolicObject> SymbolicHeap::Frame(unsigned frame, unsigned object)
{
  auto it = frames_.emplace(std::make_pair(frame, object), next_);
  if (it.second) {
    next_++;
    origins_.emplace_back(frame, object);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
ID<SymbolicObject> SymbolicHeap::Alloc(unsigned frame, CallSite *site)
{
  auto it = allocs_.emplace(std::make_pair(frame, site), next_);
  if (it.second) {
    next_++;
    origins_.emplace_back(frame, site);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
ID<Func> SymbolicHeap::Function(Func *func)
{
  auto it = funcToIDs_.emplace(func, funcToIDs_.size());
  if (it.second) {
    idToFunc_.push_back(func);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
Func &SymbolicHeap::Map(ID<Func> id)
{
  return *idToFunc_[id];
}
