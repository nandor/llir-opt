// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_summary.h"



// -----------------------------------------------------------------------------
SymbolicValue SymbolicSummary::Lookup(ConstRef<Inst> ref)
{
  auto it = values_.find(ref);
  assert(it != values_.end() && "missing value");
  return it->second;
}

// -----------------------------------------------------------------------------
void SymbolicSummary::Map(ConstRef<Inst> ref, const SymbolicValue &value)
{
  auto it = values_.emplace(ref, value);
  if (!it.second) {
    it.first->second.Merge(value);
  }
}
