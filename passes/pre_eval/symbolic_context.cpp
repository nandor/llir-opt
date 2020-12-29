// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pre_eval/symbolic_context.h"



// -----------------------------------------------------------------------------
bool SymbolicContext::Map(Inst &i, const SymbolicValue &value)
{
  assert(i.GetNumRets() == 1 && "invalid instruction");
  auto it = values_.emplace(i.GetSubValue(0), value);
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
SymbolicValue SymbolicContext::Lookup(ConstRef<Inst> inst)
{
  auto it = values_.find(inst);
  if (it == values_.end()) {
    return SymbolicValue::Unknown();
  } else {
    return it->second;
  }
}
