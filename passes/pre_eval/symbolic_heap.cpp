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
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
ID<SymbolicObject> SymbolicHeap::Frame(unsigned frame, unsigned object)
{
  auto it = frames_.emplace(std::make_pair(frame, object), next_);
  if (it.second) {
    next_++;
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
ID<SymbolicObject> SymbolicHeap::Alloc(unsigned frame, CallSite *site)
{
  auto it = allocs_.emplace(std::make_pair(frame, site), next_);
  if (it.second) {
    next_++;
  }
  return it.first->second;
}
