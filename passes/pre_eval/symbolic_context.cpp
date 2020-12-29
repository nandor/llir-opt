// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"



// -----------------------------------------------------------------------------
void SymbolicContext::Map(Inst &i, const SymbolicValue &v)
{
  assert(i.GetNumRets() == 1 && "invalid instruction");
  values_.emplace(i.GetSubValue(0), v);
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicContext::Lookup(ConstRef<Inst> inst)
{
  auto it = values_.find(inst);
  if (it == values_.end()) {
    return SymbolicValue::Unknown();
  } else {
    return it->second;
  }
}
